#include "include/ipv6.h"
#include "network/l2/mac/include/mac.h"

#include <arpa/inet.h>

#include <string>

namespace network::l3::ipv6{

using namespace network::l2::mac;

std::string Ipv6AddrToString(Ipv6Addr addr) {
    // 内部封装系统 inet_ntop，输出 RFC 5952 标准格式（压缩 ::、省略前导 0、小写）
    char buf[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, addr.data(), buf, sizeof(buf)) == nullptr) {
        return {};
    }
    return std::string(buf);
}

Ipv6Addr MacToLinkLocalAddrWithEui64(MacAddr macAddr)
{
    Ipv6Addr addr{};
    // link-local 前缀 fe80::/64
    addr[0] = 0xfe;
    addr[1] = 0x80;
    // 拼接 MAC，第一字节的 U/L 位（bit1）翻转
    addr[8] = static_cast<std::uint8_t>(macAddr[0] ^ 0x02);
    addr[9] = macAddr[1];
    addr[10] = macAddr[2];
    addr[11] = 0xff;
    addr[12] = 0xfe;
    addr[13] = macAddr[3];
    addr[14] = macAddr[4];
    addr[15] = macAddr[5];
    return addr;
}

} // namespace network::l3::ipv6
