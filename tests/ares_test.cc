#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/ns/ares.hh"
#include "my_sylar/tcp_server.hh"
#include <strstream>
#include <fstream>
#include <iostream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_domains(sylar::AresChannel::ptr channel, int idx, std::string domain) {
    int result_num = 0;
    auto ips = channel->aresGethostbyname(domain.c_str());
    for (auto& i : ips) {
        result_num++;
        if (false) {
            SYLAR_LOG_DEBUG(g_logger) << i.toString();
        }
    }
    SYLAR_LOG_DEBUG(g_logger) << "idx: " << idx << ", domain: " << domain << ", result: " << result_num;
}

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

    if (false) {
        std::ifstream in("querytest.txt");
        std::string domain;
        std::string::size_type pos;
        int i = 0;
        while (getline(in, domain) && i < 100) {
            pos = domain.find(" ", 0);
            if (pos == std::string::npos) {
                continue;
            }
            std::string real_domain(domain, 0, pos);
            //SYLAR_LOG_DEBUG(g_logger) << "domain: " << real_domain;
            i++;
            sylar::IOManager::GetThis()->schedule(std::bind(test_domains,
                                                            channel, i, real_domain));
        }
    } else {
        //sylar::IOManager::GetThis()->schedule(std::bind(test_domain, channel, 0));
        sleep(5);
        for (int i = 0; i < 10; i++) {
            sylar::IOManager::GetThis()->schedule(std::bind(test_domain,
                    channel, i));
        }
    }
}

int main() {
    sylar::IOManager iom(5, false, "io");
    //iom.addTimer(1000 * 10, sylar::MemStatics, true);
    //iom.addTimer(1000 * 10, [&iom](){
    //    std::stringstream ss;
    //    iom.dump(ss);
    //    SYLAR_LOG_DEBUG(g_logger) << ss.str();
    //}, true);
    iom.schedule(ares_test);
    iom.stop();
    return 0;
}

// iptables -A INPUT -p udp --sport 53 -j DROP
