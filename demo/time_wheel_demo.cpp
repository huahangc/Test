#include <iostream>
#include <chrono>
#include <ctime>
#include <thread>
#include "network/common/timer_wheel/include/timer_wheel.h"

using namespace network::common::timer_wheel;

static int task = 0;

void print_task(void)
{
    // 当前时间（精确到秒）
    const std::time_t t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm{};
    localtime_r(&t, &tm);   // 任务回调在 Run() 事件循环里执行，仍然用线程安全版本
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);

    std::cout << "[" << buf << "] This is Task " << task++ << '\n';
}

int main(void)
{
    std::printf("HUAHANG STARET TIMER DEMO!\n");
    BaseTimerWheel wheel {};
    // 先注册任务，再启动事件循环
    wheel.StartTimerWheel(10, print_task);
    wheel.StartTimerWheel(5, print_task);
    wheel.StartTimerWheel(3, print_task);
    wheel.StartTimerWheel(9, print_task);
    wheel.StartTimerWheel(10, print_task);
    // 12 秒后自动停止整个事件循环
    wheel.StartTimerWheel(300, [&wheel] {
        std::cout << "[stopper] 300 秒到，停止时间轮\n";
        wheel.Stop();
    });

    wheel.Run();   // 异步启动事件循环（立即返回）

    // Run() 不阻塞：main 必须自己等待，否则对象析构会把 tick 线程停掉，任务全跑不了
    // 最长任务 12 秒（stopper），多等 1 秒再退出
    std::this_thread::sleep_for(std::chrono::seconds(13));
}
