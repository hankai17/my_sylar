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
    std::string                 m_serverName;
    sylar::SocketStream::ptr    sockStream;
    sylar::Address::ptr         m_peerAddr;
};

static int udp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    Session* session = (Session*)user;
    if (session->m_serverName == "p1") {
        int ret = session->sockStream->getSocket()->sendTo(buf, len, session->m_peerAddr);
        if (false)
        SYLAR_LOG_DEBUG(g_logger) << "p1 udp_output ret: " << ret << " ";
        //<< session->m_peerAddr->toString() << "  "
        //<< std::dynamic_pointer_cast<sylar::IPv4Address>(session->m_peerAddr)->getPort();
    } else {
        int ret = session->sockStream->getSocket()->sendTo(buf, len, session->m_peerAddr);
        if (false)
        SYLAR_LOG_DEBUG(g_logger) << "p2 udp_output ret: " << ret << " ";
        //<< session->m_peerAddr->toString() << "  "
        //<< std::dynamic_pointer_cast<sylar::IPv4Address>(session->m_peerAddr)->getPort();
    }
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

static int recv_kcp(char*& buf, uint32_t& size, ikcpcb* kcp) {
    int len = ikcp_peeksize(kcp);
    if (len <= 0) {
        return 0;
    }
    char* data = new char[len];
    int r = ikcp_recv(kcp, data, len);
    if (r <= 0) {
        return -1; // may be eagain
    }
    buf = data;
    size = len;
    return size;
}

static void run(ikcpcb* kcp) {
    while (1) {
        char* buf = nullptr;
        uint32_t len = 0;
        int ret = recv_kcp(buf, len, kcp);
        if (ret <= 0) {
            //SYLAR_LOG_DEBUG(g_logger) << "run ret: " << ret;
        } else {
            std::string buff(buf, len);
            SYLAR_LOG_DEBUG(g_logger) << "p2 recv buf: " << buff;
            free(buf);
            len = 0;
        }
        usleep(1000 * 100);
    }
}

void KcpServer::startReceiver(sylar::Socket::ptr sock) {
    int once_flag = 0;
    while (!isStop()) {
        std::vector<uint8_t> buf;
        buf.resize(512 + 1);

        sylar::Address::ptr addr(new sylar::IPv4Address);
        int count = sock->recvFrom(&buf[0], buf.size(), addr);
        if (count <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "KcpServer::startReceiver UDP recv faild errno: " << errno
                                      << " strerrno: " << strerror(errno);
            return;
        }
        SYLAR_LOG_DEBUG(g_logger) << "KcpServer::starReceiver " << addr->toString() << " ret: " << count;

        if (!once_flag) {
            once_flag = 1;
            Session* s2 = new Session;
            s2->sockStream.reset(new sylar::SocketStream(sock));
            s2->m_serverName = "p2";
            s2->m_peerAddr = addr;
            kcp2 = ikcp_create(0x11223344, s2);
            ikcp_nodelay(kcp2, 1, 10, 2, 1);
            kcp2->output = udp_output;
            sylar::IOManager::GetThis()->schedule(std::bind(run, kcp2));
        }
        int kcp_id = ikcp_getconv(&buf[0]);
        SYLAR_LOG_DEBUG(g_logger) << "KcpServer::starReceiver kcp_id: " << kcp_id;

        ikcp_input(kcp2, (char*)&buf[0], count);
        ikcp_update(kcp2, sylar::GetCurrentMs());
    }
}

// ikcp_send/recv
// output/input
// read/write
// kernel

void p1_recv(ikcpcb* kcp, Session* session) {
    while (1) {
        std::vector<uint8_t> buf;
        buf.resize(512 + 1);
        sylar::Address::ptr addr(new sylar::IPv4Address);
        int count = session->sockStream->getSocket()->recvFrom(&buf[0], buf.size(), addr);
        if (count <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << "p1 recv ret < 0";
            return;
        }
        ikcp_input(kcp, (char*)&buf[0], count);
        ikcp_update(kcp, sylar::GetCurrentMs());
    }
}

void p1_server() {
    sylar::Socket::ptr sock = sylar::Socket::CreateUDPSocket();
    Session* s1 = new Session;
    s1->is_udp = true;
    s1->m_serverName = "p1";
    s1->m_peerAddr = sylar::IPAddress::Create("127.0.0.1", 9527);
    s1->sockStream.reset(new sylar::SocketStream(sock));

    kcp1 = ikcp_create(0x11223344, s1);
    ikcp_nodelay(kcp1, 1, 10, 2, 1);
    kcp1->output = udp_output;
    //sylar::IOManager::GetThis()->schedule(std::bind(run, kcp1));
    sylar::IOManager::GetThis()->schedule(std::bind(p1_recv, kcp1, s1));
    int i = 0;
    std::string buff("hello world ");

    while (1) {
        std::string buf = buff + std::to_string(i);
        i++;
        //int ret =
        ikcp_send(kcp1, buf.c_str(), buf.size());
        //SYLAR_LOG_DEBUG(g_logger) << "p1 send ret: " << ret;
        ikcp_update(kcp1, sylar::GetCurrentMs());
        usleep(1000 * 500);
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
    iom.schedule(p1_server);
    iom.stop();
    return 0;
}

/*
kcpuv:
while(1)
    epoll_wait(0)                    ikcp_input
    traversal all conn->run()        ikcp_update + while(1) { recv_kcp }     // 放入msg同步队列

    while(1) { 
        echo: 读取同步队列至空       ikcp_send echo给对端
    }
    sleep_ms(1)
    
asio:
    读回调                           ikcp_input + ikcp_recv 如果收到数据调 ikcp_send
    timer回调                        ikcp_update

*/
