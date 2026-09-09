#include <cstdint>
#include <stdexcept>
#include <sys/time.h>
#include <thread>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include "include/timer_wheel.h"

namespace network::common::timer_wheel {

namespace {

// 当前时间字符串，精确到秒（格式：2026-09-06 17:23:45）
std::string CurrentTimeStr() {
    const std::time_t t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm{};
    localtime_r(&t, &tm);   // 线程安全版本（tick 运行在独立线程里）
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

} // namespace

void BaseTimerWheel::createNeedFd()
{
    // 1. 全局 epoll fd
    m_epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollFd < 0) {
        std::perror("epoll_create1");
        throw std::runtime_error("Epoll Create Error!");
    }

    // 2. tick fd：eventfd——线程向它写 8 字节，epoll 里它就变"可读"
    //    EFD_SEMAPHORE：写 1 次 = 可读 1 次，读循环每次拿到一个 tick，不丢 tick
    m_tickFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (m_tickFd < 0) {
        close(m_epollFd);
        std::perror("timer fd create failed.");
        throw std::runtime_error("timer fd create failed");
    }
    itimerspec spec = {
        .it_interval = {
            1,
            0,
        }
    };
    spec.it_value = spec.it_interval;
    timerfd_settime(m_tickFd, 0, &spec, nullptr);
    // 3. 把 tick fd 注册进 epoll（关心"可读"）
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = m_tickFd;
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_tickFd, &ev) < 0) {
        std::perror("epoll_ctl");
        close(m_tickFd);
        close(m_epollFd);
        throw std::runtime_error("Epoll Add Tick Fd Error!");
    }
}

BaseTimerWheel::BaseTimerWheel() :
    m_timeUnit(1), m_bucketSize(256)
{
    m_buckets.resize(m_bucketSize);
    // 创建需要的fd资源
    createNeedFd();
}

BaseTimerWheel::~BaseTimerWheel()
{
    // Stop();
    if (m_tickThread.joinable()) {
        m_tickThread.join();
    }
    if (m_tickFd >= 0) {
        close(m_tickFd);
    }
    if (m_epollFd >= 0) {
        close(m_epollFd);
    }
}

void BaseTimerWheel::Stop()
{
    m_stop = true;
}

std::int32_t BaseTimerWheel::getCurTick()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const int32_t curTickUnit = ts.tv_sec; // 当前的tick次数
    return curTickUnit;
}

/**
 * @brief 向定时器中添加任务 目前只允许添加单个定时任务
 * 
 * @param interval 定时任务经过多少个时间间隔之后开始执行
 * @param cb 执行的回调函数
 * @return TD timer descriptor
 */
TD BaseTimerWheel::StartTimerWheel(std::uint32_t interval, TimerCB cb)
{
    const TD td = getNextTD();
    std::int32_t curTickUnit = getCurTick();
    const std::size_t circleIndex = (interval + curTickUnit) / m_bucketSize;
    const std::size_t bucketIndex = (interval + curTickUnit) % m_bucketSize;
    m_buckets[bucketIndex].push_back(Timer{td, cb, curTickUnit + (int32_t)interval});
    return td;
}

TD BaseTimerWheel::getNextTD()
{
    return m_curTD.fetch_add(1);
}

void BaseTimerWheel::tick()
{
    epoll_event events[8];
    while(!m_stop) {
        std::uint64_t tickTimes = 0;
        const int n = epoll_wait(m_epollFd, events, 8, -1);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd != m_tickFd) {
                continue;
            }
            std::int32_t curTick;
            std::uint32_t curIndex;
            read(m_tickFd, &tickTimes, sizeof(tickTimes));
            for (int j = 0; j < tickTimes; ++j) {
                curTick = getCurTick();
                curIndex = curTick % m_bucketSize;
                std::vector<Timer> &vec = m_buckets[curIndex];
                for (auto it = vec.begin(); it != vec.end(); ) {
                    if (it->tickTime <= curTick) {
                        it->cb();
                        it = vec.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }
}

void BaseTimerWheel::Run()
{
    m_stop = false;
    m_tickThread = std::thread(&BaseTimerWheel::tick, this);
}

}
