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
static std::atomic<uint64_t>    s_count_send_kcp_packet;
static std::atomic<uint64_t>    s_count_send_kcp_size;
uint64_t    s_count_send_kcp_size_old;

class KcpClientSession;
static int client_udp_output(const char* buf, int len, ikcpcb* kcp, void* user);

static int recv_kcp(char*& buf, uint32_t& size, ikcpcb* kcp) {
    int len = ikcp_peeksize(kcp);
    if (len <= 0) {
        return 0;
    }
    std::string buff;
    buff.resize(len);
    int r = ikcp_recv(kcp, &buff[0], len);
    if (r <= 0) {
        return -1; // may be eagain
    }
    buf = nullptr;
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
        //bool Socket::setOption(int level, int option, const void* result, size_t len) {
        int sndbuf = 65536;
        if (!m_sock->setOption(SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int)) ) {
            SYLAR_LOG_DEBUG(g_logger) << "setOption failed errno: " << errno
              << " strerrno: " << strerror(errno);
        }
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
        int ret = m_sockStream->getSocket()->sendTo(buf, len, m_peerAddr); // 这是这个kcp模型造成的错误 而非my_sylar问题
        s_count_send_kcp_size += ret;
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
        m_kcp->output = m_isUdp ? &client_udp_output : nullptr; // TCP TODO
        if (true) {
            m_kcp->interval = 1;
            m_kcp->rx_minrto = 2000;
            //ikcp_wndsize(m_kcp, 2048, 2048); // wndsize is important
            ikcp_wndsize(m_kcp, 20480, 20480); // wndsize is important
            ikcp_nodelay(m_kcp, 1, 5, 2, 1); // para2 is important  70MB/s & mem increase slow
        } else { // 40MB/s
            ikcp_nodelay(m_kcp, 1, 20, 13, 1);
            ikcp_wndsize(m_kcp, 2048, 2048);
            m_kcp->rx_minrto = 400;
            m_kcp->interval = 1;
        }
    }

    void send_msg(const std::string& msg) {
        int send_ret = ikcp_send(m_kcp, msg.c_str(), msg.size());
        if (send_ret < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "send_msg ikcp_send < 0, ret: " << send_ret;
        }
    }

    void handle_kcp_time() {
        ikcp_update(m_kcp, sylar::GetCurrentMs());
        sylar::IOManager::GetThis()->addTimer(1,
        std::bind(&KcpClientSession::handle_kcp_time, this), false);
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
                //SYLAR_LOG_DEBUG(g_logger) << "client upper recv bufsize: " << len;
            }
            usleep(1000 * 1);
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

static int client_udp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    KcpClientSession* kcs = static_cast<KcpClientSession*>(user);
    return kcs->udp_output(buf, len, kcp);
}

void static_kcp() {
    float rate = (s_count_send_kcp_size - s_count_send_kcp_size_old) * 1.0 / 10 / 1024;
    SYLAR_LOG_DEBUG(g_logger) << "sum packets: " << s_count_send_kcp_packet
      << " rate: " << rate << "KB/s";
    s_count_send_kcp_size_old = s_count_send_kcp_size;
}

void p1_test() {
    KcpClientSession::ptr kcs(new KcpClientSession(sylar::IPAddress::Create("172.16.3.98", 9527)));
    //KcpClientSession::ptr kcs(new KcpClientSession(sylar::IPAddress::Create("0.0.0.0", 9527)));
    sylar::IOManager::GetThis()->schedule(std::bind(&KcpClientSession::client_recv, kcs)); // 怎么处理生命周期
    sylar::IOManager::GetThis()->schedule(std::bind(&KcpClientSession::upper_recv, kcs));
    sylar::IOManager::GetThis()->addTimer(1,
        std::bind(&KcpClientSession::handle_kcp_time, kcs), false); // 怎么处理生命周期  // shared_from_this + stop() TODO
                                                                    // addTimer 发现过期了不会直接调 而是schedule到队列里
    sylar::IOManager::GetThis()->addTimer(1000 * 10, static_kcp, true);

    std::string buff;
    buff.resize(4 * 1024);

    while (1) {
        kcs->send_msg(buff); //模拟业务发送 64000  
        s_count_send_kcp_packet++;
        //usleep(1000 * 1);

        sylar::Fiber::YeildToReady();
        //sylar::Fiber::YeildToHold();
        if (s_count_send_kcp_packet % 10000 == 0) {
            usleep(1000 * 1);
        }
    }
        SYLAR_LOG_DEBUG(g_logger) << "done...";
        std::cout << "end" << std::endl;
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(p1_test);
    iom.stop();
    return 0;
}

//server & client's update & upper_recv set 1ms;  client upper usleep 2ms; ---> get 26MB/s and mem not increase
//server & client's update & upper_recv set 1ms;  client upper usleep 1ms; ---> get 41MB/s and mem increase slow...
