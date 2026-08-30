// demo/nng_survey_demo.cpp
// 演示 nng SURVEYOR/RESPONDENT：一对多同步调查
//   调查者广播一条调查，然后在时间窗口内同步收集所有应答（结果或错误）；
//   窗口到期（NNG_ETIMEDOUT）表示调查结束。
#include <nng/nng.h>
#include <nng/protocol/survey0/respond.h>
#include <nng/protocol/survey0/survey.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace {

constexpr const char *kUrl = "inproc://survey_demo";

// 应答者（"订阅者"）：收到调查 → 模拟处理耗时 → 回报结果或错误
void RespondentTask(const char *name, int delay_ms, const char *reply_text) {
    nng_socket rp;
    if (nng_respondent0_open(&rp) != 0) {
        return;
    }
    if (nng_dial(rp, kUrl, nullptr, 0) != 0) {
        nng_close(rp);
        return;
    }

    nng_msg *survey = nullptr;
    if (nng_recvmsg(rp, &survey, 0) != 0) {  // 等调查（阻塞）
        nng_close(rp);
        return;
    }

    const char *q = static_cast<const char *>(nng_msg_body(survey));
    std::printf("[%-8s] 收到调查: %.*s，开始处理（%d ms）...\n", name,
                static_cast<int>(nng_msg_len(survey)), q, delay_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

    nng_msg *reply = nullptr;
    nng_msg_alloc(&reply, 0);
    const std::string text = std::string(name) + ": " + reply_text;
    nng_msg_append(reply, text.c_str(), text.size() + 1);
    if (nng_sendmsg(rp, reply, 0) == 0) {
        std::printf("[%-8s] 已回报: %s\n", name, text.c_str());
    } else {
        std::printf("[%-8s] 回报失败（调查窗口已关闭）\n", name);
    }
    nng_msg_free(survey);
    nng_close(rp);
}

} // namespace

int main() {
    std::printf("== nng SURVEY 演示：广播调查，同步收集所有应答（1 秒窗口）==\n\n");

    // 调查者必须先 listen，应答者才能 dial 成功（inproc 下没有监听方 dial 会失败）
    nng_socket sv;
    if (nng_surveyor0_open(&sv) != 0) {
        return 1;
    }
    if (nng_listen(sv, kUrl, nullptr, 0) != 0) {
        nng_close(sv);
        return 1;
    }
    nng_socket_set_ms(sv, NNG_OPT_SURVEYOR_SURVEYTIME, 1000);  // 收集窗口 1 秒

    // 三个应答者：一个正常、一个报错、一个太慢赶不上窗口
    std::thread r1(RespondentTask, "worker1", 50, "OK");
    std::thread r2(RespondentTask, "worker2", 100, "ERROR: 磁盘已满");
    std::thread r3(RespondentTask, "worker3", 1500, "OK（这条赶不上窗口）");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // 等应答者连上

    nng_msg *survey = nullptr;
    nng_msg_alloc(&survey, 0);
    const char *q = "请汇报各自状态";
    nng_msg_append(survey, q, std::strlen(q) + 1);
    nng_sendmsg(sv, survey, 0);
    std::printf("[surveyor] 已广播调查: \"%s\"，开始收集应答（1 秒窗口）...\n", q);

    int count = 0;
    for (;;) {
        nng_msg *resp = nullptr;
        const int rv = nng_recvmsg(sv, &resp, 0);  // 收应答；窗口到期返回 NNG_ETIMEDOUT
        if (rv == NNG_ETIMEDOUT) {
            break;  // 调查窗口结束
        }
        if (rv != 0) {
            break;
        }
        ++count;
        std::printf("[surveyor] 收到应答 #%d: %s\n", count,
                    static_cast<const char *>(nng_msg_body(resp)));
        nng_msg_free(resp);
    }
    std::printf("[surveyor] 窗口结束，3 个应答者中共收到 %d 个应答\n", count);

    nng_close(sv);
    r1.join();
    r2.join();
    r3.join();
    std::printf("\ndone\n");
    return 0;
}
