#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include "socket.hh"

#include <string>
#include <string.h>
#include <stdlib.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_server() {
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 9527);
    sylar::Socket::ptr sock = sylar::Socket::CreateUDP(addr);
    if (!sock->bind(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "bind failed: " << sock->toString();
        return;
    }
    SYLAR_LOG_DEBUG(g_logger) << "bind success: " << sock->toString();

    while (true) {
        char buffer[1024];
        sylar::Address::ptr peer(new sylar::IPv4Address);
        int len = sock->recvFrom(buffer, 1024, peer);
        if (len > 0) {
            buffer[len] = '\0';
            SYLAR_LOG_DEBUG(g_logger) << "server recvfrom peer: " << peer->toString() << ", len: " << len;
            len = sock->sendTo(buffer, len, peer);
            if (len < 0) {
                SYLAR_LOG_DEBUG(g_logger) << "server sendto peer: " << peer->toString() << ", failed"
                << " error: " << errno << " strerrno: " << strerror(errno);
            }
        }
    }
};

void test_client() {
    sylar::IPv4Address::ptr peer = sylar::IPv4Address::Create("127.0.0.1", 9527);
    sylar::Socket::ptr sock = sylar::Socket::CreateUDP(peer);

    if (true) {
        char buffer[1024] = "hello world";
        int len = sock->sendTo(buffer, strlen(buffer), peer);
        if (len < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "client sendto peer: " << peer->toString() << ", failed"
                                      << " error: " << errno << " strerrno: " << strerror(errno);
        }
        memset(buffer, 0, 1024);
        len = sock->recvFrom(buffer, 1024, peer);
        if (len < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "client recvfrom peer: " << peer->toString() << ", failed"
                                      << " error: " << errno << " strerrno: " << strerror(errno);
            return;
        }
        SYLAR_LOG_DEBUG(g_logger) << "client recv: " << buffer;
    }
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(test_server);
    iom.schedule(test_client);
    iom.stop();
    return 0;
}