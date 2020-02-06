#include "socket.hh"
#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include <vector>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_socket() {
    SYLAR_LOG_DEBUG(g_logger) << "test_socket begin";
    std::vector<sylar::Address::ptr> addrs;
    //sylar::Address::Lookup(addrs, "www.ifeng.com");
    sylar::Address::Lookup(addrs, "www.baidu.com");
    if (addrs.size() < 1) {
        SYLAR_LOG_DEBUG(g_logger) << "resolve error";
        return;
    }
    sylar::IPAddress::ptr addr = std::dynamic_pointer_cast<sylar::IPAddress>(addrs[0]);
    if (!addr) {
        SYLAR_LOG_DEBUG(g_logger) << "dynamic_cast failed";
        return;
    }
    // Must IPv4

    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    addr->setPort(80);
    SYLAR_LOG_DEBUG(g_logger) << addr->toString();

    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect failed " << addr->toString();
        return;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "connect success";
    }

    const char buff[] = "GET / HTTP/1.1\r\n\r\n";
    //const char buff[] = "GET / HTTP/1.1\r\nHost: www.ifeng.com\r\n\r\n";
    int ret = sock->send(buff, sizeof(buff));
    if (ret <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "send failed ret: " << ret;
        return;
    }

    std::string buffs;
    buffs.resize(4096);
    ret = sock->recv(&buffs[0], buffs.size());
    if (ret <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "recv failed ret: " << ret;
        return;
    }
    buffs.resize(ret);
    //SYLAR_LOG_DEBUG(g_logger) << buffs;
    SYLAR_LOG_DEBUG(g_logger) << "sock.use_count: " << sock.use_count();
}

int main() {
    sylar::IOManager iom(1, false);
    iom.schedule(test_socket);
    iom.stop();
    return 0;
}