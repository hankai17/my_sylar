#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/stream.hh"
#include "my_sylar/tcp_server.hh"
#include "my_sylar/thread.hh"
#include "my_sylar/address.hh"
#include "my_sylar/util.hh"
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
    bool                        is_udp;
    sylar::SocketStream::ptr    sockStream;
    sylar::Address::ptr         m_peerAddr;
};

// KCP的下层协议输出函数，KCP需要发送数据时会调用它
static int udp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    Session* session = (Session*)user;
    int ret = session->sockStream->getSocket()->sendTo(buf, len, session->m_peerAddr);
    SYLAR_LOG_DEBUG(g_logger) << "udp_output ret: " << ret;
    return 0;
}

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

static int recv_kcp(char*& buf, uint32_t& size) {
    int len = ikcp_peeksize(kcp2);
    if (len <= 0) {
        return 0;
    }
    char* data = new char[len];
    int r = ikcp_recv(kcp2, data, len);
    if (r <= 0) {
        return -1; // may be eagain
    }
    buf = data;
    size = len;
    return size;
}

static void run() {
    while (1) {
        char* buf = nullptr;
        uint32_t len = 0;
        int ret = recv_kcp(buf, len);
        if (ret <= 0) {
            //SYLAR_LOG_DEBUG(g_logger) << "run ret: " << ret;
        } else {
            SYLAR_LOG_DEBUG(g_logger) << "run ret: " << ret
            << " buf: " << buf;
            free(buf);
            len = 0;
        }
        usleep(1000 * 10);
    }
}

void KcpServer::startReceiver(sylar::Socket::ptr sock) {
    int once_flag = 0;
    while (!isStop()) {
        SYLAR_LOG_DEBUG(g_logger) << "while KcpServer::starReceiver ";
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
            sylar::IOManager::GetThis()->schedule(run);
        }

        // recv data from peer/kcp
        ikcp_input(kcp2, (char*)&buf[0], count);
        // update
        ikcp_update(kcp1, 10);
    }
}

// ikcp_send/recv
// output/input
// read/write
// kernel

void p1_test() {
    sylar::Socket::ptr sock = sylar::Socket::CreateUDPSocket();
    Session* s1 = new Session;
    s1->is_udp = true;
    s1->m_peerAddr = sylar::IPAddress::Create("127.0.0.1", 9527);
    s1->sockStream.reset(new sylar::SocketStream(sock));

    kcp1 = ikcp_create(0x11223344, s1);
    kcp1->output = udp_output;
    ikcp_send(kcp1, "hello world", 10);

    while (1) {
        //SYLAR_LOG_DEBUG(g_logger) << "update and sleep";
        ikcp_update(kcp1, sylar::GetCurrentMs());
        usleep(1000 * 10);
    }
}

void kcpServerStart() {
    sylar::Address::ptr addr = sylar::IPAddress::Create("127.0.0.1", 9527);
    KcpServer::ptr ks(new KcpServer);
    ks->bind(addr, true);
    ks->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(kcpServerStart);
    iom.schedule(p1_test);
    iom.stop();
    return 0;
}
