// nng_pubsub_demo.cpp
// 演示 nng 进程内 PUB/SUB：1 个发布者线程 → 2 个订阅者线程，异步分发
//
// 看点：
//   - 发布者 nng_send 立即返回，不等订阅者处理（异步）
//   - 两个订阅者故意"慢慢"消费（每条 300ms），验证发布者不会被拖住
//   - topic 是消息的前缀："demo." + 内容
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

constexpr const char *kTopic = "demo.";

void PublisherTask(const char *url) {
    nng_socket pub;
    if (nng_pub0_open(&pub) != 0) {
        std::fprintf(stderr, "pub open failed\n");
        return;
    }
    if (nng_listen(pub, url, nullptr, 0) != 0) {
        std::fprintf(stderr, "pub listen failed\n");
        nng_close(pub);
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // 等订阅者完成连接（PUB 会丢弃"没人订阅"时的消息）

    for (int i = 1; i <= 5; ++i) {
        const std::string body = "event-" + std::to_string(i);

        nng_msg *msg = nullptr;
        nng_msg_alloc(&msg, 0);
        nng_msg_append(msg, kTopic, 5);                    // topic 前缀
        nng_msg_append(msg, body.c_str(), body.size() + 1); // 消息内容

        nng_sendmsg(pub, msg, 0);  // 发布后立即返回，不等任何订阅者
        std::printf("[pub ] sent: %s\n", body.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    nng_close(pub);
}

void SubscriberTask(const char *url, const char *name) {
    nng_socket sub;
    if (nng_sub0_open(&sub) != 0) {
        std::fprintf(stderr, "%s open failed\n", name);
        return;
    }
    if (nng_dial(sub, url, nullptr, 0) != 0) {
        std::fprintf(stderr, "%s dial failed\n", name);
        nng_close(sub);
        return;
    }
    nng_socket_set(sub, NNG_OPT_SUB_SUBSCRIBE, "", 0);      // 订阅全部 topic
    nng_socket_set_ms(sub, NNG_OPT_RECVTIMEO, 500);         // 空闲 500ms 就超时

    // 发布者停止后再等 5 秒就退出
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        nng_msg *msg = nullptr;
        const int rv = nng_recvmsg(sub, &msg, 0);  // 阻塞等消息，超时返回 NNG_ETIMEDOUT
        if (rv == NNG_ETIMEDOUT) {
            continue;
        }
        if (rv != 0) {
            break;
        }

        const char *data = static_cast<const char *>(nng_msg_body(msg));
        const std::size_t len = nng_msg_len(msg);
        std::printf("[%-4s] got: %.*s\n", name, static_cast<int>(len), data);

        nng_msg_free(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));  // 模拟"慢消费者"——发布者不会被拖住
    }
    nng_close(sub);
}

} // namespace

int main() {
    const char *url = "inproc://pubsub_demo";
    std::printf("== nng PUB/SUB 演示：1 个发布者 → 2 个订阅者（异步分发）==\n");
    std::printf("注意观察：pub 连发 5 条立即返回，两个 sub 按自己的慢节奏各收各的\n\n");

    std::thread pub(PublisherTask, url);
    std::thread sub1(SubscriberTask, url, "sub1");
    std::thread sub2(SubscriberTask, url, "sub2");

    pub.join();
    sub1.join();
    sub2.join();

    std::printf("\ndone\n");
    return 0;
}
