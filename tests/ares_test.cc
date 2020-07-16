#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/ns/ares.hh"
#include "my_sylar/tcp_server.hh"
#include <strstream>
#include <fstream>
#include <iostream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_domain(sylar::AresChannel::ptr channel, int idx) {
    int result_num = 0;
    SYLAR_LOG_DEBUG(g_logger) << "idx: " << idx << " start";
    std::string domain("www.ifeng.com");
    auto ips = channel->aresGethostbyname(domain.c_str());
    for (auto& i : ips) {
        result_num++;
        SYLAR_LOG_DEBUG(g_logger) << idx << " " << i.toString();
    }
    SYLAR_LOG_DEBUG(g_logger) << "idx: " << idx << ", domain: " << domain << ", result: " << result_num;
}

void ares_test() {
    SYLAR_LOG_DEBUG(g_logger) << "in ares test";
    sylar::AresChannel::ptr channel(new sylar::AresChannel);
    channel->init();
    channel->start();

    for (int i = 0; i < 1; i++) {
        sylar::IOManager::GetThis()->schedule(std::bind(test_domain,
                channel, i));
    }
    sylar::IOManager::GetThis()->addTimer(2000, std::bind(test_domain, channel, 1), true);
}

int main() {
    sylar::IOManager iom(5, false, "io");
    iom.schedule(ares_test);
    iom.stop();
    return 0;
}

