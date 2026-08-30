#include "ipv6.h"
#include "network/l2/mac/mac.h"

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
    Ipv6Addr addr {};
    // link local地址的前两个字节为fe80:xx
    addr[0] = 0xfe;
    addr[1] = 0x80;
    // 拼接mac地址
    addr[8] = macAddr[0]; // 该地址的低位第二个Bit需要翻转
    addr[8] = addr[8] ^ 0x2;
    addr[9] = macAddr[1];
    addr[10] = macAddr[2];
    addr[11] = 0xff;
    addr[12] = 0xfe;
    addr[13] = macAddr[3];
    addr[14] = macAddr[4];
    addr[15] = macAddr[5];
    return addr;
}

}
