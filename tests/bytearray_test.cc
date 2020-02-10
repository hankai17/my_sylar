#include "bytearray.hh"
#include "log.hh"
#include "macro.hh"

#include <vector>
#include <stdlib.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test() {
    std::vector<int8_t> vec;
    int len = 100;
    for (int i = 0; i < len; i++) {
        vec.push_back(rand());
    }
    sylar::ByteArray::ptr ba(new sylar::ByteArray(1));
    for (const auto& i : vec) {
        ba->writeFint8(i);
    }
    ba->setPosition(0);
    for (size_t i = 0; i < vec.size(); i++) {
        int8_t v = ba->readFint8();
        SYLAR_ASSERT(v == vec[i]);
    }
    SYLAR_ASSERT(ba->getReadSize() == 0);
    SYLAR_LOG_DEBUG(g_logger) << "writeFint8" << " readFint8"
    << " int8_t " << "100 1 size: " << ba->getSize();
}

int main() {
    test();
    return 0;
}