#include "http/http_parser.hh"
#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include "socket.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_socket() {
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

    //addr = sylar::IPv4Address::Create("127.0.0.1");
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    addr->setPort(80);
    SYLAR_LOG_DEBUG(g_logger) << addr->toString();

    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect failed " << addr->toString()
                                  << " errno: " << strerror(errno);
        return;
    } else {
        //SYLAR_LOG_DEBUG(g_logger) << "connect success";
    }

    const char buff[] = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\n\r\n";
    //const char buff[] = "GET /2.html HTTP/1.1\r\nHost: 127.0.0.1:90\r\n\r\n";
    int ret = sock->send(buff, strlen(buff));
    if (ret <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "send failed ret: " << ret;
        return;
    } else if (ret == (int)strlen(buff)) {
        SYLAR_LOG_DEBUG(g_logger) << " send request ok " << ret;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "send incompletable ret: " << ret;
    }

    // We should check whether buff send all ok. so we SHOULD SHOULD SHOULD check ret == strlen(buff)

    std::string buffs;
    buffs.resize(4096);
    size_t buff_offset = 0;
    sylar::http::HttpResponseParser parser;

    while( (ret = sock->recv(&buffs[buff_offset], buffs.size())) > 0) {
        SYLAR_LOG_DEBUG(g_logger) << "ret: " << ret;
        if (!parser.isFinished()) {
            size_t parsed = parser.execute(&buffs[0], buffs.size());
            SYLAR_LOG_DEBUG(g_logger) << "parsed: " << parsed;
        }
        buff_offset += ret;

        buffs.resize(buffs.size() * 2);
    }

    if (ret == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "recv: err: " << strerror(errno);
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "recv done ret: " << ret;
    }

}

int main() {
    sylar::IOManager iom(1, false, "io");
    for (int i = 0; i < 1; i++) {
        iom.schedule(test_socket);
    }
    iom.stop();
    return 0;
}
