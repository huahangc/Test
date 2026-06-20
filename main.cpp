#include <bitset>
#include <iostream>

using std::bitset;
using std::cout;

namespace test1 {
    void HelloWorld()
    {
        cout << "Helle world!" << '\n';
    }
}

namespace test2 {
    void HelloWOrld()
    {
        cout << "Hello WOrld!" << '\n';
    }
}

void do_something();

int main() {    do_something();
    test1::HelloWorld();
    test2::HelloWOrld();
    bitset<20> bit_set {0b0000'0001};
    cout << "BitSet Is " << bit_set << '\n';
    cout << "BItSet Size is " << bit_set.size() << '\n';
    cout << "BitSet Flip is " << bit_set.flip() << '\n';
    return 0;
}
