#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
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

BaseTimerWheel::BaseTimerWheel() :
    m_timeUnit(1), m_maxTimeUnit(256)
{
    m_bucketSize = m_maxTimeUnit / m_timeUnit;
    if (m_maxTimeUnit % m_timeUnit != 0) {
        throw std::runtime_error("maxTimeUnit must be divisible by timeUnit");
    }
    m_buckets.resize(m_bucketSize);

    // 1. 全局 epoll fd
    m_epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollFd < 0) {
        std::perror("epoll_create1");
        throw std::runtime_error("Epoll Create Error!");
    }

    // 2. tick fd：eventfd——线程向它写 8 字节，epoll 里它就变"可读"
    //    EFD_SEMAPHORE：写 1 次 = 可读 1 次，读循环每次拿到一个 tick，不丢 tick
    m_tickFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
    if (m_tickFd < 0) {
        std::perror("eventfd");
        close(m_epollFd);
        throw std::runtime_error("EventFd Create Error!");
    }

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

BaseTimerWheel::~BaseTimerWheel()
{
    Stop();
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
    // 唤醒可能阻塞在 epoll_wait 里的 Run()：往 eventfd 写一个 tick
    const std::uint64_t one = 1;
    if (m_tickFd >= 0) {
        (void)write(m_tickFd, &one, sizeof(one));
    }
}

TD BaseTimerWheel::StartTimerWheel(std::uint32_t interval, std::function<void()> cb)
{
    // 不做取模：定时时长超出时间轮最大范围直接报错
    if (interval + m_curTimeUnit >= m_bucketSize) {
        throw std::out_of_range("timer interval exceeds timer wheel max time");
    }
    const TD td = getNextTD();
    const std::size_t idx = interval + m_curTimeUnit;
    m_buckets[idx].push_back(Timer{td, cb});
    return td;
}

TD BaseTimerWheel::getNextTD()
{
    return m_curTD.fetch_add(1);
}

void BaseTimerWheel::tick()
{
    // tick 线程：每秒往 eventfd 写 8 字节 = 给 epoll "发一条消息"
    while (!m_stop) {
        std::this_thread::sleep_for(std::chrono::seconds(m_timeUnit));
        const std::uint64_t one = 1;
        if (write(m_tickFd, &one, sizeof(one)) != sizeof(one)) {
            std::perror("tick write");
            m_stop = true;
            break;
        }
        std::printf("[tick thread] 已向 epoll 发送 tick: %s\n", CurrentTimeStr().c_str());
    }
}

void BaseTimerWheel::Run()
{
    // 4. 启动 tick 线程：只负责每秒"向 epoll 发消息"，不干活
    m_tickThread = std::thread(&BaseTimerWheel::tick, this);
    // 主事件循环：阻塞等 epoll 通知，tick fd 可读 = 时间前进一格
    while (!m_stop) {
        epoll_event events[8];
        const int n = epoll_wait(m_epollFd, events, 8, -1);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd != m_tickFd) {
                continue;
            }
            // 排空 eventfd：EFD_SEMAPHORE 下每 read 一次消耗一个 tick
            std::uint64_t tick = 0;
            while (read(m_tickFd, &tick, sizeof(tick)) == sizeof(tick)) {
                if (m_stop) {
                    break;   // Stop() 唤醒用的那一次 tick，不推进时间轮
                }
                m_curTimeUnit = m_curTimeUnit + 1;   // 不做取模

                // 超出最大范围：直接报错并停止
                if (m_curTimeUnit >= m_bucketSize) {
                    std::fprintf(stderr,
                                 "[timer_wheel] 时间轮已推进到最大值 %zu，超出范围，停止运行\n",
                                 m_bucketSize);
                    m_stop = true;
                    break;
                }

                // 把当前桶整体"搬走"（桶原地变空），执行到期任务
                std::vector<Timer> timers;
                timers.swap(m_buckets[m_curTimeUnit]);
                for (const Timer &timer : timers) {
                    timer.cb();
                }
            }
        }
    }
}

}
