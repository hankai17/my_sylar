#include "log.hh"
#include "iomanager.hh"
#include "ns/ares.hh"
#include "tcp_server.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_domains(sylar::AresChannel::ptr channel, int idx) {
    std::string domain("www.ifeng.com");
    auto ips = channel->aresGethostbyname(domain.c_str());
    for (auto& i : ips) {
        SYLAR_LOG_DEBUG(g_logger) << i.toString();
    }
    SYLAR_LOG_DEBUG(g_logger) << "idx: " << idx << ", domain: " << domain << ", test done";
}

void ares_test() {
    SYLAR_LOG_DEBUG(g_logger) << "in ares test";
    sylar::AresChannel::ptr channel(new sylar::AresChannel);
    channel->init();
    channel->start();
    for (int i = 0; i < 10; i++) {
        sylar::IOManager::GetThis()->schedule(std::bind(test_domains,
                channel, i));
    }
}

void ticker() {
    sylar::IOManager::GetThis()->addTimer(1000, [](){
        //SYLAR_LOG_DEBUG(g_logger) << "ticker...";
        }, true);
}

int main() {
    sylar::IOManager iom(1, false, "io");
    //iom.schedule(ticker);
    iom.schedule(ares_test);
    iom.stop();
    return 0;
}