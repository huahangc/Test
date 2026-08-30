// network/common/event_bus.h
// 进程内事件通知机制（观察者模式 / 事件总线）
//
// 用法：
//   EventBus<int> bus;                          // 事件类型为 int（也可以是自定义结构体）
//   auto token = bus.Subscribe([](const int &v) { ... });   // 订阅
//   bus.Publish(42);                            // 发布：广播给所有订阅者
//   bus.Unsubscribe(token);                     // 取消订阅
//
// 特点：
//   - 模板化：事件类型任意（int、std::string、自定义 struct 均可）
//   - 线程安全：Subscribe/Unsubscribe/Publish 内部有互斥锁保护
//   - Publish 先拷贝订阅者快照再逐个回调，因此回调里可以安全地订阅/取消订阅
#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace network::common {

template <typename Event>
class EventBus {
public:
    using Handler = std::function<void(const Event &)>;
    using Token = std::size_t;

    // 订阅事件，返回可用于取消订阅的 token
    Token Subscribe(Handler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.emplace_back(++next_token_, std::move(handler));
        return next_token_;
    }

    // 取消订阅；token 有效返回 true，重复取消返回 false
    bool Unsubscribe(Token token) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if (it->first == token) {
                handlers_.erase(it);
                return true;
            }
        }
        return false;
    }

    // 发布事件：调用所有订阅者（按订阅顺序）
    void Publish(const Event &event) {
        std::vector<std::pair<Token, Handler>> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = handlers_;  // 拷贝快照，避免回调里改动列表导致迭代器失效
        }
        for (const auto &[token, handler] : snapshot) {
            handler(event);
        }
    }

    std::size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::pair<Token, Handler>> handlers_;
    Token next_token_ = 0;
};

} // namespace network::common
