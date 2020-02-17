#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include "socket.hh"


// Echo tcp: TO see Why need tcp_server.hh/cc

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();


// Wrong module: Because both acceptfd and sockfd are sharing one fiber
void test_tcp1() {
    sylar::IPv4Address::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 9527);
    sylar::Socket::ptr sock(sylar::Socket::CreateTCP(addr));
    if (!sock->bind(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "bind: " << addr->toString()
        << " errno: " << errno << " " << strerror(errno);
        return;
    }
    sock->listen(1024);

    sylar::Socket::ptr new_sock;
    while (1) {
        if ((new_sock = sock->accept()) == nullptr) {
            SYLAR_LOG_DEBUG(g_logger) << "accept none";
            continue;
        }
        std::stringstream ss;
        new_sock->dump(ss);
        SYLAR_LOG_DEBUG(g_logger) << "get client: " << ss.str();

        std::string buffer;
        buffer.resize(1024);
        int ret = new_sock->recv(&buffer[0], buffer.size());
        if (ret == 0) {
            SYLAR_LOG_DEBUG(g_logger) << "peer closed";
            new_sock->close();
            continue;
        }
        buffer.resize(ret);
        SYLAR_LOG_DEBUG(g_logger) << "recv: " << buffer;
        new_sock->send(&buffer[0], ret);
        new_sock->close();
    }
}

void only_do_tcp(sylar::Socket::ptr sock) {
    std::stringstream ss;
    sock->dump(ss);
    SYLAR_LOG_DEBUG(g_logger) << "get client: " << ss.str();

    std::string buffer;
    buffer.resize(1024);
    int ret = sock->recv(&buffer[0], buffer.size());
    if (ret == 0) {
        SYLAR_LOG_DEBUG(g_logger) << "peer closed";
    }
    buffer.resize(ret);
    SYLAR_LOG_DEBUG(g_logger) << "recv: " << buffer;
    sock->send(&buffer[0], ret);
    sock->close();
}

void only_accept() {
    sylar::IPv4Address::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 9527);
    sylar::Socket::ptr sock(sylar::Socket::CreateTCP(addr));
    if (!sock->bind(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "bind: " << addr->toString()
                                  << " errno: " << errno << " " << strerror(errno);
        return;
    }
    sock->listen(1024);
    sylar::Socket::ptr new_sock;
    while (1) {
        if ((new_sock = sock->accept()) == nullptr) {
            SYLAR_LOG_DEBUG(g_logger) << "accept none";
            continue;
        }
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        iom->schedule(std::bind(only_do_tcp, new_sock));
    }
}

int main() {
    sylar::IOManager iom(1, false, "io");
    //iom.schedule(test_tcp1);
    iom.schedule(only_accept);
    iom.stop();
    return 0;
}
