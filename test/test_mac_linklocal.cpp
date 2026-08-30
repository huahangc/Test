// test/test_mac_linklocal.cpp
// MacToLinkLocalAddrWithEui64（MAC → EUI-64 link-local）的单元测试（doctest）
//
// 推导规则（RFC 4291 §2.5.1 / RFC 2464 §4）：
//   1. 前缀 fe80::/64（前 8 字节 = fe 80 00 00 00 00 00 00）
//   2. 接口标识 = MAC 中间插入 0xff 0xfe，并把第一个字节的 U/L 位（掩码 0x02）翻转
// 期望字符串为 Ipv6AddrToString（RFC 5952 标准压缩格式）的输出
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "network/l3/ipv6/ipv6.h"
#include "network/l2/mac/mac.h"

#include <arpa/inet.h>

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>

using namespace network::l3::ipv6;
using namespace network::l2::mac;

namespace {

MacAddr makeMac(std::initializer_list<std::uint8_t> bytes) {
    MacAddr m{};
    std::size_t i = 0;
    for (auto b : bytes) {
        m[i++] = b;
    }
    return m;
}

Ipv6Addr makeIpv6(std::initializer_list<std::uint8_t> bytes) {
    Ipv6Addr a{};
    std::size_t i = 0;
    for (auto b : bytes) {
        a[i++] = b;
    }
    return a;
}

// 伪随机递推，产生下一个"随机"MAC
void nextRandomMac(MacAddr &m) {
    for (auto &b : m) {
        b = static_cast<std::uint8_t>(b * 31u + 7u);
    }
}

} // namespace

TEST_CASE("已知 MAC 转换为 link-local 地址（字节 + 字符串双校验）") {
    struct Case {
        MacAddr mac;
        Ipv6Addr expected;
        const char *expectedStr;
    };
    const Case cases[] = {
        {makeMac({0x00, 0x00, 0x00, 0x00, 0x00, 0x00}),
         makeIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00, 0x00}),
         "fe80::200:ff:fe00:0"},
        {makeMac({0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e}),
         makeIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x1a, 0x2b, 0xff, 0xfe, 0x3c, 0x4d, 0x5e}),
         "fe80::21a:2bff:fe3c:4d5e"},
        {makeMac({0x52, 0x54, 0x00, 0x12, 0x35, 0x02}),  // QEMU 默认
         makeIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x50, 0x54, 0x00, 0xff, 0xfe, 0x12, 0x35, 0x02}),
         "fe80::5054:ff:fe12:3502"},
        {makeMac({0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}),
         makeIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0xa8, 0xbb, 0xcc, 0xff, 0xfe, 0xdd, 0xee, 0xff}),
         "fe80::a8bb:ccff:fedd:eeff"},
        {makeMac({0x02, 0x00, 0x00, 0x00, 0x00, 0x01}),  // U/L 位已置位
         makeIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x00, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00, 0x01}),
         "fe80::ff:fe00:1"},
        {makeMac({0x00, 0x00, 0xc0, 0x15, 0xea, 0x5e}),  // RFC 2464 官方示例
         makeIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x00, 0xc0, 0xff, 0xfe, 0x15, 0xea, 0x5e}),
         "fe80::200:c0ff:fe15:ea5e"},
    };

    for (const auto &c : cases) {
        const Ipv6Addr actual = MacToLinkLocalAddrWithEui64(c.mac);
        CHECK(std::memcmp(actual.data(), c.expected.data(), 16) == 0);
        CHECK_EQ(Ipv6AddrToString(actual), std::string(c.expectedStr));
    }
}

TEST_CASE("随机 MAC 性质校验（不依赖手写期望值）") {
    MacAddr m = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
    for (int i = 0; i < 100; ++i) {
        const Ipv6Addr a = MacToLinkLocalAddrWithEui64(m);

        // fe80::/64 前缀
        CHECK_EQ(a[0], 0xfe);
        CHECK_EQ(a[1], 0x80);
        for (int j = 2; j <= 7; ++j) {
            CHECK_EQ(a[j], 0);
        }
        // U/L 位翻转 + ff:fe 标记 + 其余字节原样
        CHECK_EQ(a[8], static_cast<std::uint8_t>(m[0] ^ 0x02));
        CHECK_EQ(a[9], m[1]);
        CHECK_EQ(a[10], m[2]);
        CHECK_EQ(a[11], 0xff);
        CHECK_EQ(a[12], 0xfe);
        CHECK_EQ(a[13], m[3]);
        CHECK_EQ(a[14], m[4]);
        CHECK_EQ(a[15], m[5]);

        // round-trip：输出必须能被 inet_pton 解析回相同字节
        const std::string s = Ipv6AddrToString(a);
        std::uint8_t parsed[16];
        CHECK_EQ(inet_pton(AF_INET6, s.c_str(), parsed), 1);
        CHECK(std::memcmp(parsed, a.data(), 16) == 0);

        nextRandomMac(m);
    }
}
