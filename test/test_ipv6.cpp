// test/test_ipv6.cpp
// Ipv6AddrToString 的单元测试（doctest）
//
// Ipv6AddrToString 内部封装系统 inet_ntop，输出 RFC 5952 标准格式
//（压缩 ::、省略前导 0、小写，IPv4 映射地址用点分十进制）。
//
// 校验点：
//   1. 与期望的标准格式字符串一致
//   2. 输出长度在合理范围（2 ~ 45）
//   3. round-trip：输出能被 inet_pton 解析，且解析回与原输入相同的 16 字节
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "network/l3/ipv6/ipv6.h"

#include <arpa/inet.h>

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>

using namespace network::l3::ipv6;

namespace {

Ipv6Addr makeIpv6(std::initializer_list<std::uint8_t> bytes) {
    Ipv6Addr a{};
    std::size_t i = 0;
    for (auto b : bytes) {
        a[i++] = b;
    }
    return a;
}

} // namespace

TEST_CASE("常见地址输出为 RFC 5952 标准格式") {
    CHECK_EQ(Ipv6AddrToString(makeIpv6({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})),
             std::string("::"));
    CHECK_EQ(Ipv6AddrToString(makeIpv6({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1})),
             std::string("::1"));
    CHECK_EQ(Ipv6AddrToString(makeIpv6({0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1})),
             std::string("2001:db8::1"));
    CHECK_EQ(Ipv6AddrToString(makeIpv6({0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0, 0, 0, 0,
                                      0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34})),
             std::string("2001:db8:85a3::8a2e:370:7334"));
    CHECK_EQ(Ipv6AddrToString(makeIpv6({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff,
                                      0xc0, 0x00, 0x02, 0x80})),
             std::string("::ffff:192.0.2.128"));  // IPv4 映射地址输出点分十进制
    CHECK_EQ(Ipv6AddrToString(makeIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1})),
             std::string("fe80::1"));
    CHECK_EQ(Ipv6AddrToString(makeIpv6({0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1})),
             std::string("ff02::1"));
    CHECK_EQ(Ipv6AddrToString(makeIpv6({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff})),
             std::string("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
}

TEST_CASE("输出长度在合理范围且可 round-trip") {
    const Ipv6Addr addrs[] = {
        makeIpv6({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}),                         // ::
        makeIpv6({0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88}), // 2001:4860:4860::8888
        makeIpv6({0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
                  0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}),                          // ff:ff:...:ff
    };
    for (const auto &a : addrs) {
        const std::string s = Ipv6AddrToString(a);
        // 压缩格式长度不定：最短 "::"（2 字符），最长 45（INET6_ADDRSTRLEN - 1）
        CHECK(s.size() >= 2u);
        CHECK(s.size() <= 45u);

        std::uint8_t parsed[16];
        CHECK_EQ(inet_pton(AF_INET6, s.c_str(), parsed), 1);
        CHECK(std::memcmp(parsed, a.data(), 16) == 0);
    }
}
