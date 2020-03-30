#include "log.hh"
#include "iomanager.hh"
#include "ns/ares.hh"
#include "tcp_server.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test() {
    SYLAR_LOG_DEBUG(g_logger) << "in ares test";
    sylar::AresChannel::ptr channel(new sylar::AresChannel);
    channel->init();
    channel->start();

    std::string domain("www.ifeng.com");
    auto ips = channel->aresGethostbyname(domain.c_str());
    for (auto& i : ips) {
        SYLAR_LOG_DEBUG(g_logger) << i.toString();
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
    iom.schedule(test);
    iom.stop();
    return 0;
}