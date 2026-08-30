#include "network/l2/mac/mac.h"

#include <cstdio>

// 这个函数定义在 second.cpp，由 main.cpp 调用
void printFromSecond() {
    using namespace network::l2::mac;

    std::printf("from second.cpp: %s\n",
                MacAddrToString({0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}).c_str());
    std::printf("kMacAddrLen @ second: %p\n", (const void *)&kMacAddrLen);
}
