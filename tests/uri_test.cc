#include "log.hh"
#include "uri.hh"
#include "iomanager.hh"
#include "address.hh"
#include "socket.hh"
#include "util.hh"

#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/time.h>
#include<arpa/inet.h>
#include<netinet/tcp.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

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

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(test1);
    iom.stop();
    return 0;
}

