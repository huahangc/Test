#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "network/l2/mac/include/mac.h"

namespace network::l3::ipv6 {

inline constexpr std::size_t kIpv6AddrLen = 16;

/// IPv6 地址：16 字节数组，按显示顺序（网络序）存储，无需字节序转换
using Ipv6Addr = std::array<std::uint8_t, kIpv6AddrLen>;

/// 把 IPv6 地址格式化为 RFC 5952 标准字符串（如 ::1、2001:db8::1），内部封装 inet_ntop
std::string Ipv6AddrToString(Ipv6Addr addr);

/// MAC → EUI-64 link-local IPv6 地址（均按字节序存储）
Ipv6Addr MacToLinkLocalAddrWithEui64(network::l2::mac::MacAddr macAddr);

} // namespace network::l3::ipv6
