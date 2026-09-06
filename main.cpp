#include "network/l2/mac/include/mac.h"

#include <cstdio>

void printFromSecond(); // 定义在 second.cpp

int main() {
    using namespace network::l2::mac;

    // 字节序：按显示顺序直接填
    MacAddr broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    MacAddr local     = {0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e};

    std::printf("broadcast: %s\n", MacAddrToString(broadcast).c_str());
    std::printf("local:     %s\n", MacAddrToString(local).c_str());
    std::printf("kMacAddrLen @ main:   %p\n", (const void *)&kMacAddrLen);

    printFromSecond();
    return 0;
}
