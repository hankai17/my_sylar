#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/stream.hh"
#include "my_sylar/tcp_server.hh"
#include "my_sylar/thread.hh"
#include "my_sylar/address.hh"
extern "C" {
#include "my_sylar/utils/ikcp.h"
}
#include <iostream>
#include <memory>
#include <vector>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
ikcpcb* kcp1 = nullptr;
ikcpcb* kcp2 = nullptr;

class Session {
public:
    sylar::SocketStream::ptr sockStream;
};

class KcpServer : public sylar::UdpServer {
public:
    typedef std::shared_ptr<KcpServer> ptr;
    typedef sylar::RWMutex RWMutexType;
    KcpServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
            sylar::IOManager* receiver = sylar::IOManager::GetThis());
    ~KcpServer() {}

protected:
    void startReceiver(sylar::Socket::ptr sock) override;
private:
    RWMutexType			m_mutex;
};

KcpServer::KcpServer(sylar::IOManager* worker, sylar::IOManager* receiver):
UdpServer(worker, receiver) {
}

static int udp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    Session* session = (Session*)user;
    session->sockStream->write(buf, len);
    return 0;
}

void KcpServer::startReceiver(sylar::Socket::ptr sock) {
    int once_flag = 0;
    while (!isStop()) {
        std::vector<uint8_t> buf;
        buf.resize(512 + 1);
        int count = sock->recv(&buf[0], buf.size());
        if (count <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "KcpServer::startReceiver UDP recv faild errno: " << errno
                                      << " strerrno: " << strerror(errno);
            return;
        }
        SYLAR_LOG_DEBUG(g_logger) << "KcpServer::starReceiver ret: " << count;

        if (!once_flag) {
            once_flag = 1;
            Session* s2 = new Session;
            s2->sockStream.reset(new sylar::SocketStream(sock));
            kcp2 = ikcp_create(0x11223344, s2);
            kcp2->output = udp_output;
        }

        // recv data from peer/kcp
        ikcp_input(kcp2, (char*)&buf[0], count);
        // update
    }
}

void kcpServerStart() {
    sylar::Address::ptr addr = sylar::IPAddress::Create("127.0.0.1", 9527);
    KcpServer::ptr ks(new KcpServer);
    ks->bind(addr);
    ks->start();
}

// ikcp_send/recv
// output/input
// read/write
// kernel

void test() {
    sylar::Address::ptr addr = sylar::IPAddress::Create("127.0.0.1", 9527);
    sylar::Socket::ptr sock = sylar::Socket::CreateUDPSocket();

    Session* s1 = new Session;
    s1->sockStream.reset(new sylar::SocketStream(sock));
    kcp1 = ikcp_create(0x11223344, s1);
    kcp1->output = udp_output;

    while (1) {
        // send data to peer/kcp
        ikcp_send(kcp1, "hello world", 11);
        // update
    }

}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(kcpServerStart);
    iom.schedule(test);
    iom.stop();
    return 0;
}
