// demo/nng_reqrep_demo.cpp
// 演示 nng REQ/REP：同步请求-应答
//   请求方 nng_sendmsg 发出请求后，nng_recvmsg 阻塞等待，
//   处理者处理完把【结果或错误】作为应答发回，请求方同步拿到。
#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace {

constexpr const char *kUrl = "inproc://reqrep_demo";

// 处理者（服务端）：收到请求 → 模拟处理耗时 → 把结果或错误作为应答发回
void ServerTask() {
    nng_socket rep;
    if (nng_rep0_open(&rep) != 0) {
        std::fprintf(stderr, "rep open failed\n");
        return;
    }
    if (nng_listen(rep, kUrl, nullptr, 0) != 0) {
        std::fprintf(stderr, "rep listen failed\n");
        nng_close(rep);
        return;
    }
    std::printf("[rep ] 服务端就绪，监听 %s\n", kUrl);

    for (int i = 0; i < 3; ++i) {
        nng_msg *req = nullptr;
        if (nng_recvmsg(rep, &req, 0) != 0) {
            break;  // 等请求（阻塞）
        }

        const char *data = static_cast<const char *>(nng_msg_body(req));
        const std::size_t len = nng_msg_len(req);
        std::printf("[rep ] 收到请求: %.*s\n", static_cast<int>(len), data);

        std::this_thread::sleep_for(std::chrono::milliseconds(300));  // 模拟处理耗时

        nng_msg *reply = nullptr;
        nng_msg_alloc(&reply, 0);
        if (std::strcmp(data, "fail") == 0) {
            const char *err = "ERROR: 处理失败";
            nng_msg_append(reply, err, std::strlen(err) + 1);
            std::printf("[rep ] 处理出错，返回错误应答: %s\n", err);
        } else {
            const std::string ok = "OK: " + std::string(data, len);
            nng_msg_append(reply, ok.c_str(), ok.size() + 1);
            std::printf("[rep ] 处理完成，返回应答: %s\n", ok.c_str());
        }
        nng_sendmsg(rep, reply, 0);  // 应答 → 请求方立刻同步收到
        nng_msg_free(req);
    }
    nng_close(rep);
}

} // namespace

int main() {
    std::printf("== nng REQ/REP 演示：请求方同步等待处理结果/错误 ==\n\n");

    std::thread server(ServerTask);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 等服务端就绪

    nng_socket req;
    if (nng_req0_open(&req) != 0) {
        return 1;
    }
    if (nng_dial(req, kUrl, nullptr, 0) != 0) {
        nng_close(req);
        return 1;
    }

    const char *requests[] = {"hello", "fail", "done"};
    for (const char *body : requests) {
        nng_msg *msg = nullptr;
        nng_msg_alloc(&msg, 0);
        nng_msg_append(msg, body, std::strlen(body) + 1);

        const auto t0 = std::chrono::steady_clock::now();
        nng_sendmsg(req, msg, 0);   // 发请求
        nng_msg *reply = nullptr;
        nng_recvmsg(req, &reply, 0);  // ★ 阻塞，直到处理者回结果/错误
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();

        std::printf("[req ] \"%s\" → \"%s\"（同步等待 %lld ms）\n", body,
                    static_cast<const char *>(nng_msg_body(reply)),
                    static_cast<long long>(ms));
        nng_msg_free(reply);
    }

    nng_close(req);
    server.join();
    std::printf("\ndone\n");
    return 0;
}
