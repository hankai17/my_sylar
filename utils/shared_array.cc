#include <memory>
#include <iostream>
#include <list>
#include <fstream>
#include <stdio.h>
#include <boost/shared_array.hpp>

using namespace std;

template <typename T>
void nop(const T&) {}

void test() {
    char buf[10] = "123";
    std::shared_ptr<char> b;
    b.reset(buf, [](){});

    printf("ori's addr: %p\n", buf);
    printf("buf's addr: %p\n", b.get());

    char buf1[40] = "1234";
    boost::shared_array<char> a;
    a.reset(buf1, &nop<char*>);
    printf("ori's addr: %p\n", buf1);
    printf("buf's addr: %p\n", a.get());

    while(1);
}

int main() {
    test();
    return 0;
}
