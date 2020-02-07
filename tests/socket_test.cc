#include "socket.hh"
#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include <vector>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_socket(int index) {
    //SYLAR_LOG_DEBUG(g_logger) << "index begin: " << index;
    /*
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
    */
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1");
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    addr->setPort(90);
    //SYLAR_LOG_DEBUG(g_logger) << addr->toString();

    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "index: " << index << "  connect failed " << addr->toString();
        return;
    } else {
        //SYLAR_LOG_DEBUG(g_logger) << "connect success";
    }

    //const char buff[] = "GET / HTTP/1.1\r\n\r\n";
    //const char buff[] = "GET / HTTP/1.1\r\nHost: www.ifeng.com\r\n\r\n";
    const char buff[] = "GET /2.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    //int ret = sock->send(buff, sizeof(buff));
    int ret = sock->send(buff, strlen(buff));
    if (ret <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "send failed ret: " << ret;
        return;
    }

    std::string buffs;
    buffs.resize(4096);

    while( (ret = sock->recv(&buffs[0], buffs.size())) > 0) {
        //SYLAR_LOG_DEBUG(g_logger) << "ret: " << ret;
        buffs.clear();
        buffs.resize(4096);
    }

    if (ret == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "index: " << index << "  err: " << strerror(errno);
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "index: " << index << " done";
    }

    //SYLAR_LOG_DEBUG(g_logger) << buffs;
    //SYLAR_LOG_DEBUG(g_logger) << "sock.use_count: " << sock.use_count();
}

int main() {
    sylar::IOManager iom(1, false);
    for (int i = 0; i < 5000; i++) {
        iom.schedule(std::bind(test_socket, i));
    }
    iom.stop();
    return 0;
}