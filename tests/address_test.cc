#include "log.hh"
#include "address.hh"

#include <vector>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_ipv4() {
    auto addr = sylar::IPv4Address::Create("128.0.0.1");
    addr->setPort(801);
    if (addr) {
        SYLAR_LOG_DEBUG(g_logger) <<  addr->toString();
    }

    auto addr1 = sylar::IPv6Address::Create("2000::4");
    if (addr1) {
        SYLAR_LOG_DEBUG(g_logger) <<  addr1->toString();
    }

    auto addr2 = sylar::IPAddress::Create("127.0.0.1", 8880);
    if (addr2) {
        SYLAR_LOG_DEBUG(g_logger) <<  addr2->toString();
    }
}

void test_resolve() {
    std::vector<sylar::Address::ptr> addrs;
    SYLAR_LOG_DEBUG(g_logger) << "before resolve";
    bool v = sylar::Address::Lookup(addrs, "www.ifeng.com");
    SYLAR_LOG_DEBUG(g_logger) << "after resolve";
    if (!v) {
        SYLAR_LOG_ERROR(g_logger) << "Address::Lookup err";
        return;
    }
    for (const auto& i : addrs) {
        SYLAR_LOG_DEBUG(g_logger) << i->toString();
    }
}

int main() {
    test_ipv4();
    test_resolve();
    return 0;
}