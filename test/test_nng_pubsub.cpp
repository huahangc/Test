// test/test_nng_pubsub.cpp
// nng 进程内 PUB/SUB 的单元测试（doctest）
//
// 覆盖：
//   1. 一个发布者 → 多个订阅者，全部收到消息（topic + 内容正确）
//   2. topic 过滤：订阅者只收到自己订阅的 topic
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace {

// 发送一条 "topic + 内容" 的消息
int SendMsg(nng_socket pub, const char *topic, const char *body) {
    nng_msg *msg = nullptr;
    int rv = nng_msg_alloc(&msg, 0);
    if (rv != 0) {
        return rv;
    }
    rv = nng_msg_append(msg, topic, std::strlen(topic));
    if (rv == 0) {
        rv = nng_msg_append(msg, body, std::strlen(body) + 1);
    }
    if (rv == 0) {
        rv = nng_sendmsg(pub, msg, 0);
    }
    return rv;
}

void SleepMs(long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace

TEST_CASE("一个发布者 → 两个订阅者，都能收到消息") {
    const char *url = "inproc://test_pubsub_basic";

    nng_socket pub, sub1, sub2;
    REQUIRE_EQ(nng_pub0_open(&pub), 0);
    REQUIRE_EQ(nng_listen(pub, url, nullptr, 0), 0);
    REQUIRE_EQ(nng_sub0_open(&sub1), 0);
    REQUIRE_EQ(nng_sub0_open(&sub2), 0);
    REQUIRE_EQ(nng_dial(sub1, url, nullptr, 0), 0);
    REQUIRE_EQ(nng_dial(sub2, url, nullptr, 0), 0);
    REQUIRE_EQ(nng_socket_set(sub1, NNG_OPT_SUB_SUBSCRIBE, "", 0), 0);
    REQUIRE_EQ(nng_socket_set(sub2, NNG_OPT_SUB_SUBSCRIBE, "", 0), 0);

    // 等连接建立（PUB 在订阅者连上之前发的消息会被丢弃）
    SleepMs(100);

    REQUIRE_EQ(SendMsg(pub, "demo.", "hello"), 0);

    // 两个订阅者都收到，topic 和内容都正确
    nng_socket subs[2] = {sub1, sub2};
    for (int i = 0; i < 2; ++i) {
        nng_socket_set_ms(subs[i], NNG_OPT_RECVTIMEO, 2000);  // 防止测试卡死
        nng_msg *rmsg = nullptr;
        REQUIRE_EQ(nng_recvmsg(subs[i], &rmsg, 0), 0);

        const char *data = static_cast<const char *>(nng_msg_body(rmsg));
        const std::size_t len = nng_msg_len(rmsg);
        CHECK_EQ(len, 5u + 6u);                        // "demo." + "hello\0"
        CHECK_EQ(std::memcmp(data, "demo.", 5), 0);
        CHECK_EQ(std::strcmp(data + 5, "hello"), 0);
        nng_msg_free(rmsg);
    }

    nng_close(pub);
    nng_close(sub1);
    nng_close(sub2);
}

TEST_CASE("topic 过滤：只收到订阅的 topic") {
    const char *url = "inproc://test_pubsub_filter";

    nng_socket pub, sub;
    REQUIRE_EQ(nng_pub0_open(&pub), 0);
    REQUIRE_EQ(nng_listen(pub, url, nullptr, 0), 0);
    REQUIRE_EQ(nng_sub0_open(&sub), 0);
    REQUIRE_EQ(nng_dial(sub, url, nullptr, 0), 0);
    REQUIRE_EQ(nng_socket_set(sub, NNG_OPT_SUB_SUBSCRIBE, "topicA.", 7), 0);

    SleepMs(100);

    // 先发 topicB.（应被过滤掉），再发 topicA.
    REQUIRE_EQ(SendMsg(pub, "topicB.", "b"), 0);
    REQUIRE_EQ(SendMsg(pub, "topicA.", "a"), 0);

    // 第一收：topicA. 的消息
    nng_socket_set_ms(sub, NNG_OPT_RECVTIMEO, 2000);
    nng_msg *rmsg = nullptr;
    REQUIRE_EQ(nng_recvmsg(sub, &rmsg, 0), 0);
    const char *data = static_cast<const char *>(nng_msg_body(rmsg));
    CHECK_EQ(std::memcmp(data, "topicA.a", 8), 0);
    nng_msg_free(rmsg);

    // 第二收：topicB. 被过滤了，应该超时
    rmsg = nullptr;
    CHECK_EQ(nng_recvmsg(sub, &rmsg, 0), NNG_ETIMEDOUT);

    nng_close(pub);
    nng_close(sub);
}
