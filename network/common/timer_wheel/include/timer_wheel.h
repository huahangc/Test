/*
* 时间轮
*/

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace network::common::timer_wheel {

using TD = std::int32_t; // timer descriptor 时间轮描述符
using Timer = struct {
    TD td;
    std::function<void(void)> cb;
};


class BaseTimerWheel {
public:
    /**
     * @brief Construct a new Base Timer Wheel object
     * 
     */
    BaseTimerWheel ();
    /**
     * @brief 析构：停止时间轮驱动线程并等待其退出
     */
    ~BaseTimerWheel();
    /**
     * @brief 向定时器新增任务
     * 
     * @param interval 多少时间间隔之后开始执行
     * @param cb 回调函数 void(void)
     * @return TD 唯一的标志符
     */
    TD StartTimerWheel(std::uint32_t interval, std::function<void()> cb);
    /**
     * @brief 停止任务
     * 
     * @param twd 时间任务
     * @return TD 成功返回传入的twd，失败返回-1
     */
    TD StopTimerWheel(TD twd);

    /**
     * @brief 主事件循环：阻塞等待 epoll 通知，
     *        tick fd 可读时推进时间轮一格并执行到期任务
     */
    void Run();

    /**
     * @brief 停止时间轮：唤醒 Run 并让 tick 线程退出
     */
    void Stop();

private:
    /* 时间单位 */
    const std::size_t m_timeUnit;
    /* 最大运行时间 */
    const std::size_t m_maxTimeUnit;
    /* 内存桶大小 */
    std::size_t m_bucketSize;
    /* 当前的时间 */
    std::atomic<std::size_t> m_curTimeUnit {0};
    /* 当前的td值 */
    std::atomic<std::size_t> m_curTD {1};
    /* list */
    std::vector<std::vector<Timer>> m_buckets;
    /* tick线程 */
    std::thread m_tickThread;
    /* 停止标志 */
    std::atomic<bool> m_stop{false};
    /* global epoll fd */
    int m_epollFd = -1;
    /* tick epoll fd */
    int m_tickFd = -1;
    /**
     * @brief 获取下一个时间轮描述符
     * 
     * @return TD 
     */
    TD getNextTD();
    void tick();
};
} // namespace network common timer wheel
