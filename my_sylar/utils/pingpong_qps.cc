#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/address.hh"
#include "my_sylar/socket.hh"
#include "my_sylar/util.hh"
#include <iostream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test() {
    sylar::IPAddress::ptr addr = sylar::IPAddress::Create("127.0.0.1", 9527);
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect faild addr: " << addr->toString();
        return;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "connect succeed: " << addr->toString();
    }
    std::string str("hello world");
    std::string recv_buf;
    recv_buf.resize(11);
    int ret = 0;
    int i = 0;

    sylar::IOManager::GetThis()->addTimer(1000, [sock](){
        sock->close();
    });

    while (1) {
        if (sock->send(str.c_str(), str.size()) <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << "send failed errno: " << errno
            << " strerror: " << strerror(errno);
            break;
        }
        if ((ret = sock->recv(&recv_buf[0], str.size())) <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << "recv failed errno: " << errno
                                      << " strerror: " << strerror(errno);
            break;
        }
        if (ret != 11) {
            SYLAR_LOG_DEBUG(g_logger) << "recv less 11";
            break;
        }
        //SYLAR_LOG_DEBUG(g_logger) << "recv success";

        i++;
    }
    SYLAR_LOG_DEBUG(g_logger) << "i: " << i;
}

int main() {
    sylar::IOManager iom(1, false, "io");
    for (int i = 0; i < 1; i++) {
        iom.schedule(test);
    }
    iom.stop();
    return 0;
}
