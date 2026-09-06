#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace network::l2::mac {

inline constexpr std::size_t kMacAddrLen = 6;

/// MAC 地址：6 字节数组，按显示顺序（线缆序/网络序）存储，无需字节序转换
using MacAddr = std::array<std::uint8_t, kMacAddrLen>;

/// 把 MAC 地址格式化为 "aa:bb:cc:dd:ee:ff" 形式
inline std::string MacAddrToString(const MacAddr &addr) {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                  addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return std::string(buf);
}

} // namespace network::l2::mac
