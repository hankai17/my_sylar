#include "my_sylar/log.hh"
#include "my_sylar/uri.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/address.hh"
#include "my_sylar/socket.hh"
#include "my_sylar/util.hh"
#include "my_sylar/ns/ares.hh"

#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/time.h>
#include<arpa/inet.h>
#include<netinet/tcp.h>

#define closesocket close

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
sylar::AresChannel::ptr channel = nullptr;

void test() {
    //sylar::Uri::ptr uri = sylar::Uri::Create("http://www.ifeng.com/path/to/1.html?k=v&k1=v1#frag=1");
    sylar::Uri::ptr uri = sylar::Uri::Create("https://www.ifeng.com/path/to/瓜枣/1.html?k=v&k1=v1&k2=瓜枣#frag=瓜枣");
    SYLAR_LOG_DEBUG(g_logger) << "toString: " << uri->toString();

    SYLAR_LOG_DEBUG(g_logger) << "uifo: " << uri->getUserinfo();
    SYLAR_LOG_DEBUG(g_logger) << "sche: " << uri->getScheme();
    SYLAR_LOG_DEBUG(g_logger) << "host: " << uri->getHost();
    SYLAR_LOG_DEBUG(g_logger) << "port: " << uri->getPort();
    SYLAR_LOG_DEBUG(g_logger) << "path: " << uri->getPath();
    SYLAR_LOG_DEBUG(g_logger) << "qery: " << uri->getQuery();
    SYLAR_LOG_DEBUG(g_logger) << "frag: " << uri->getFragment();

    auto addr = uri->createAddress();
    SYLAR_LOG_DEBUG(g_logger) << addr->toString();
    return;
}

void test1() {
    sylar::IPAddress::ptr addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80");
    if (addr) {
        SYLAR_LOG_DEBUG(g_logger) << "addr: " << addr->toString();
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "get addr fail";
        return;;
    }
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect faild addr: " << addr->toString();
        return;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "connect succeed: " << addr->toString();
    }

    uint64_t ts = sylar::GetCurrentUs();
    for (size_t i = 0; i < 10000000000ul; i++) {
        if (int err = sock->getError()) {
            SYLAR_LOG_DEBUG(g_logger) << "err: " << err
            << " strerror: " << strerror(errno);
            break;
        }

        struct tcp_info tcp_info;
        if(!sock->getOption(IPPROTO_TCP, TCP_INFO, tcp_info)) {
            SYLAR_LOG_INFO(g_logger) << "err";
            break;
        }
        if(tcp_info.tcpi_state != TCP_ESTABLISHED) {
            SYLAR_LOG_INFO(g_logger)
                    << " state=" << (int)tcp_info.tcpi_state;
            break;
        }
        static int batch = 10000000;
        if(i && (i % batch) == 0) {
            uint64_t ts2 = sylar::GetCurrentUs();
            SYLAR_LOG_INFO(g_logger) << "i=" << i << " used: " << ((ts2 - ts) * 1.0 / batch) << " us";
            ts = ts2;
        }
    }
}

void test3() {
    //sylar::IPAddress::ptr addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80");
    sylar::IPAddress::ptr addr = sylar::IPAddress::Create("61.135.169.121", 80);
    if (addr) {
        SYLAR_LOG_DEBUG(g_logger) << "addr: " << addr->toString();
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "get addr fail";
        return;;
    }
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect faild addr: " << addr->toString();
        return;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "connect succeed: " << addr->toString();
    }

    //uint64_t ts = sylar::GetCurrentUs();
}

void test1_1(sylar::Socket::ptr sock) {
    std::string buf;
    buf.resize(1024);

    sock->recv(&buf[0], buf.size());
    SYLAR_LOG_DEBUG(g_logger) << "end test1_1";
}

void test2() {
    sylar::IPAddress::ptr addr = nullptr;
    if (0) {
        addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80"); // 不知道为什么加上以后就变成阻塞的了
    } else {
        std::string domain("www.ifeng.com");
        if (channel == nullptr) {
            SYLAR_LOG_DEBUG(g_logger) << "channel is nullptr sleep 5s...";
            sleep(5);
        }
        auto ips = channel->aresGethostbyname(domain.c_str());
        for (auto& i : ips) {
            SYLAR_LOG_DEBUG(g_logger) << i.toString();
        }
        SYLAR_LOG_DEBUG(g_logger) << "domain: " << domain << ", test done";

        addr = std::dynamic_pointer_cast<sylar::IPAddress>(
              sylar::Address::Create(ips[0].getAddr(), ips[0].getAddrLen())
              );
    }

    if (addr) {
        SYLAR_LOG_DEBUG(g_logger) << "addr: " << addr->toString();
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "get addr fail";
        return;;
    }
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "fd: " << sock->getSocket() << " connect faild addr: " << addr->toString();
        return;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "fd: " << sock->getSocket() << " connect succeed: " << addr->toString();
    }

    sylar::IOManager::GetThis()->schedule(std::bind(test1_1, sock));
    SYLAR_LOG_DEBUG(g_logger) << "after schedule...";

    sleep(10);
    sock->close(); //这里手动close 底层会触发test_1中的读事件 (so dangerous!)
    SYLAR_LOG_DEBUG(g_logger) << "after sock->close: " << addr->toString();
}

void ares_test() {
    SYLAR_LOG_DEBUG(g_logger) << "in ares test";
    channel.reset(new sylar::AresChannel);
    channel->init();
    channel->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    //iom.schedule(test1);
    //iom.schedule(test2);
    //iom.schedule(ares_test);
    iom.schedule(test3);
    iom.stop();
    return 0;
}

