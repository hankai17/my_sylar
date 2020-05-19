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
#include <atomic>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class KcpClientSession;
class KcpServerSession;
static int client_udp_output(const char* buf, int len, ikcpcb* kcp, void* user);
static int server_udp_output(const char* buf, int len, ikcpcb* kcp, void* user);

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

class SessionManager {
public:
    static uint16_t GetId() {
        static std::atomic<uint16_t>    s_current_id;
        if (s_current_id == 65535) {
            s_current_id = 0;
            return 65535;
        }
        return ++s_current_id;
    }

    //static std::atomic<uint16_t>    s_current_id;
    //static std::atomic<uint64_t>    s_count_send_kcp_packet;
    //static std::atomic<uint64_t>    s_count_send_kcp_size;
};

class KcpClientSession {
public:
    typedef std::shared_ptr<KcpClientSession> ptr;

    KcpClientSession(sylar::IPAddress::ptr peerAddr, bool is_udp = true,
                     const std::string& client_name = "p1") :
            m_peerAddr(peerAddr),
            m_isUdp(is_udp),
            m_serverName(client_name) {
        m_sock = is_udp ? sylar::Socket::CreateUDPSocket() : nullptr; // TCP TODO
        m_sockStream.reset(new sylar::SocketStream(m_sock));
        //get m_kcp_id from mgr
        init_kcp();
    }

    ~KcpClientSession() {
        if (m_kcp) {
            ikcp_release(m_kcp);
        }
    }

    int udp_output(const char*buf, int len, ikcpcb* kcp) {
        int ret = m_sockStream->getSocket()->sendTo(buf, len, m_peerAddr);
        if (false) {
            SYLAR_LOG_DEBUG(g_logger) << m_serverName << " udp_output ret: " << ret << " ";
            //<< m_peerAddr->toString() << "  "
            //<< std::dynamic_pointer_cast<sylar::IPv4Address>(m_peerAddr)->getPort();
        }
        return 0;
    }

    void init_kcp() {
        m_kcp_id = SessionManager::GetId();
        m_kcp = ikcp_create(m_kcp_id, this);
        m_kcp->interval = 1;
        m_kcp->rx_minrto = 5;
        m_kcp->output = m_isUdp ? &client_udp_output : nullptr; // TCP TODO
        ikcp_nodelay(m_kcp, 1, 10, 2, 1);
    }

    void send_msg(const std::string& msg) {
        int send_ret = ikcp_send(m_kcp, msg.c_str(), msg.size());
        if (send_ret < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "send_msg ikcp_send < 0, ret: " << send_ret;
        }
    }

    void handle_kcp_time() {
        ikcp_update(m_kcp, sylar::GetCurrentMs());
    }

    void upper_recv() {
        ikcpcb* kcp = m_kcp;
        while (1) {
            char* buf = nullptr;
            uint32_t len = 0;
            int ret = recv_kcp(buf, len, kcp);
            if (ret <= 0) {
                //SYLAR_LOG_DEBUG(g_logger) << "upper_recv ret: " << ret;
            } else {
                std::string buff(buf, len);
                SYLAR_LOG_DEBUG(g_logger) << "client upper recv bufsize: " << buff.size();
                free(buf);
                len = 0;
            }
            usleep(1000 * 5);
        }
    }

    void client_recv() {
        while (1) {
            std::vector<uint8_t> buf;
            buf.resize(64 * 1024);
            sylar::Address::ptr addr(new sylar::IPv4Address);
            int count = m_sockStream->getSocket()->recvFrom(&buf[0], buf.size(), addr);
            if (count <= 0) {
                SYLAR_LOG_DEBUG(g_logger) << "server recv ret < 0";
                return;
            }
            // check peerAddr TODO
            ikcp_input(m_kcp, (char*)&buf[0], count);
            ikcp_update(m_kcp, sylar::GetCurrentMs());
        }
    }

    ikcpcb* getKcp() {
        return m_kcp;
    }

private:
    sylar::IPAddress::ptr       m_peerAddr;
    bool                        m_isUdp;
    std::string                 m_serverName;
    sylar::Socket::ptr          m_sock;
    sylar::SocketStream::ptr    m_sockStream;
    ikcpcb*                     m_kcp;
    uint16_t                    m_kcp_id;
};

class KcpServerSession {
public:
    typedef std::shared_ptr<KcpServerSession> ptr;
    KcpServerSession(sylar::IPAddress::ptr peerAddr, uint16_t kcp_id, bool is_udp = true,
                     const std::string& client_name = "p2") :
            m_peerAddr(peerAddr),
            m_kcp_id(kcp_id),
            m_isUdp(is_udp),
            m_serverName(client_name) {
        init_kcp();
    }

    ~KcpServerSession() {
        if (m_kcp) {
            ikcp_release(m_kcp);
        }
    }

    int udp_output(const char*buf, int len, ikcpcb* kcp) {
        int ret = m_sock->sendTo(buf, len, m_peerAddr);
        if (false) {
            SYLAR_LOG_DEBUG(g_logger) << m_serverName << " udp_output ret: " << ret << " ";
            //<< m_peerAddr->toString() << "  "
            //<< std::dynamic_pointer_cast<sylar::IPv4Address>(m_peerAddr)->getPort();
        }
        return 0;
    }

    void send_msg(const std::string& msg) {
        int send_ret = ikcp_send(m_kcp, msg.c_str(), msg.size());
        if (send_ret < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "send_msg ikcp_send < 0, ret: " << send_ret;
        }
    }

    void handle_kcp_time() {
        ikcp_update(m_kcp, sylar::GetCurrentMs());
    }

    void upper_recv() {
        ikcpcb *kcp = m_kcp;
        while (1) {
            char *buf = nullptr;
            uint32_t len = 0;
            int ret = recv_kcp(buf, len, kcp);
            if (ret <= 0) {
                //SYLAR_LOG_DEBUG(g_logger) << "upper_recv ret: " << ret;
            } else {
                std::string buff(buf, len);
                SYLAR_LOG_DEBUG(g_logger) << "server upper recv bufsize: " << buff.size();
                send_msg(buff);
                free(buf);
                len = 0;
            }
            usleep(1000 * 5);
        }
    }

    void init_kcp() {
        m_kcp = ikcp_create(m_kcp_id, this);
        m_kcp->output = m_isUdp ? &server_udp_output : nullptr; // TCP TODO
        m_kcp->interval = 1;
        m_kcp->rx_minrto = 5;
        ikcp_nodelay(m_kcp, 1, 10, 2, 1);
    }

    ikcpcb* getKcp() {
        return m_kcp;
    }

    void setSock(sylar::Socket::ptr sock) {
        m_sock = sock;
    }

private:
    sylar::IPAddress::ptr       m_peerAddr;
    uint16_t                    m_kcp_id;
    bool                        m_isUdp;
    std::string                 m_serverName;
    sylar::Socket::ptr          m_sock;
    sylar::SocketStream::ptr    m_sockStream;
    ikcpcb*                     m_kcp;
};

class KcpServer : public sylar::UdpServer {
public:
    typedef std::shared_ptr<KcpServer> ptr;
    typedef sylar::RWMutex RWMutexType;
    KcpServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
              sylar::IOManager* receiver = sylar::IOManager::GetThis()) :
            UdpServer(worker, receiver) {
    }
    ~KcpServer() {}

protected:
    void startReceiver(sylar::Socket::ptr sock) {
        while (!isStop()) {
            std::vector<uint8_t> buf;
            buf.resize(64 * 1024);

            sylar::Address::ptr addr(new sylar::IPv4Address);
            int count = sock->recvFrom(&buf[0], buf.size(), addr);
            if (count <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "KcpServer::startReceiver UDP recv faild errno: " << errno
                                          << " strerrno: " << strerror(errno);
                return;
            }
            //SYLAR_LOG_DEBUG(g_logger) << "KcpServer::starReceiver " << addr->toString() << " ret: " << count;

            int kcp_id = ikcp_getconv(&buf[0]);
            //SYLAR_LOG_DEBUG(g_logger) << "KcpServer::starReceiver kcp_id: " << kcp_id;
            KcpServerSession::ptr kss = nullptr;

            auto it = m_sessions.find(kcp_id);
            if (it == m_sessions.end()) {
                kss = KcpServerSession::ptr(
                        new KcpServerSession(
                                sylar::IPAddress::Create(
                                        std::dynamic_pointer_cast<sylar::IPv4Address>(addr)->toString().c_str(),
                                        std::dynamic_pointer_cast<sylar::IPv4Address>(addr)->getPort()
                                ) , kcp_id));
                kss->setSock(sock);
                m_sessions[kcp_id] = kss;
                sylar::IOManager::GetThis()->schedule(std::bind(&KcpServerSession::upper_recv, kss));
                sylar::IOManager::GetThis()->addTimer(10,
                                          std::bind(&KcpServerSession::handle_kcp_time, kss), true);
            } else {
                kss = it->second;
            }


            ikcp_input(kss->getKcp(), (char*)&buf[0], count);
            //ikcp_update(kss->getKcp(), sylar::GetCurrentMs());
        }
    }

private:
    RWMutexType			                        m_mutex;
    std::map<uint16_t, KcpServerSession::ptr>   m_sessions;
};


// ikcp_send/recv
// output/input
// read/write
// kernel

static int client_udp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    KcpClientSession* kcs = static_cast<KcpClientSession*>(user);
    return kcs->udp_output(buf, len, kcp);
}

static int server_udp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    KcpServerSession* kss = static_cast<KcpServerSession*>(user);
    return kss->udp_output(buf, len, kcp);
}

void p1_test() {
    KcpClientSession::ptr kcs(new KcpClientSession(sylar::IPAddress::Create("127.0.0.1", 9527)));
    sylar::IOManager::GetThis()->schedule(std::bind(&KcpClientSession::client_recv, kcs)); // 怎么处理生命周期
    sylar::IOManager::GetThis()->schedule(std::bind(&KcpClientSession::upper_recv, kcs));
    sylar::IOManager::GetThis()->addTimer(100,
                                          std::bind(&KcpClientSession::handle_kcp_time, kcs), true); // 怎么处理生命周期  // shared_from_this + stop() TODO

    std::string buff;
    buff.resize(64 * 1024);

    while (1) {
        kcs->send_msg(buff); //模拟业务发送
        usleep(1000 * 20);
    }
}

void kcpServerStart() {
    sylar::Address::ptr addr = sylar::IPAddress::Create("0.0.0.0", 9527);
    KcpServer::ptr ks(new KcpServer);
    ks->bind(addr, true);
    ks->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(kcpServerStart);
    //iom.schedule(p1_test);
    //iom.schedule(p1_test);
    iom.stop();
    return 0;
}
