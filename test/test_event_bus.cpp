// test/test_event_bus.cpp
// network::common::EventBus（进程内事件通知）的单元测试（doctest）
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "network/common/event_bus.h"

#include <string>

using namespace network::common;

TEST_CASE("订阅后发布能收到事件") {
    EventBus<int> bus;
    int received = -1;
    bus.Subscribe([&](const int &v) { received = v; });

    bus.Publish(42);
    CHECK_EQ(received, 42);
}

TEST_CASE("多个订阅者都能收到同一次发布") {
    EventBus<std::string> bus;
    std::string a, b;
    bus.Subscribe([&](const std::string &s) { a = s; });
    bus.Subscribe([&](const std::string &s) { b = s; });

    bus.Publish("hello");
    CHECK_EQ(a, "hello");
    CHECK_EQ(b, "hello");
}

TEST_CASE("取消订阅后不再收到事件") {
    EventBus<int> bus;
    int count = 0;
    const auto token = bus.Subscribe([&](const int &) { ++count; });

    bus.Publish(1);
    CHECK(bus.Unsubscribe(token));
    bus.Publish(2);

    CHECK_EQ(count, 1);                  // 只收到第一次
    CHECK_FALSE(bus.Unsubscribe(token)); // 重复取消返回 false
}

TEST_CASE("自定义事件结构体") {
    struct LinkEvent {
        bool up;
        int ifindex;
    };
    EventBus<LinkEvent> bus;
    LinkEvent last{false, 0};
    bus.Subscribe([&](const LinkEvent &e) { last = e; });

    bus.Publish(LinkEvent{true, 3});
    CHECK(last.up);
    CHECK_EQ(last.ifindex, 3);
}

TEST_CASE("回调里可以安全地取消订阅") {
    EventBus<int> bus;
    int received = 0;
    EventBus<int>::Token token = 0;
    token = bus.Subscribe([&](const int &v) {
        received = v;
        bus.Unsubscribe(token);  // 发布期间取消自己，不应崩溃
    });

    bus.Publish(7);
    CHECK_EQ(received, 7);
    CHECK_EQ(bus.Size(), 0u);
}
