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
#include <unordered_map>

#include "kcp_utils.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
static std::atomic<uint64_t>    s_count_send_kcp_packet;
static std::atomic<uint64_t>    s_count_send_kcp_size;
uint64_t    s_count_send_kcp_size_old;

class KcpServerSession;
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

class KcpServerSession {
public:
    typedef std::shared_ptr<KcpServerSession> ptr;
    KcpServerSession(sylar::Address::ptr peerAddr, uint32_t kcp_id, bool is_udp = true,
                     const std::string& client_name = "p2", uint32_t recv_time = 0) :
            m_peerAddr(peerAddr),
            m_kcp_id(kcp_id),
            m_isUdp(is_udp),
            m_serverName(client_name),
            m_lastRecvPtime(0) {
        init_kcp(kcp_id);
    }

    ~KcpServerSession() {
        clean();
    }

    void clean() {
        SYLAR_LOG_DEBUG(g_logger) << "clean kcp: " << m_kcp_id;
        std::string disconn_msg = make_disconnect_pack(m_kcp_id);
        int ret = m_sock->sendTo(disconn_msg.c_str(), disconn_msg.size(), m_peerAddr);
        if (ret <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << " senTo peer ret: " << ret;
        }

        if (m_kcp) {
            ikcp_release(m_kcp);
        }
        m_kcp = nullptr;
    }

    void init_kcp(uint32_t id) {
        m_kcp_id = id;
        m_kcp = ikcp_create(m_kcp_id, this);
        m_kcp->output = m_isUdp ? &server_udp_output : nullptr; // TCP TODO
        if (true) {
            m_kcp->interval = 1;
            m_kcp->rx_minrto = 100;
            ikcp_wndsize(m_kcp, 1024 * 10, 1024 * 10);
            ikcp_nodelay(m_kcp, 1, 5, 2, 1);
        } else {
            ikcp_nodelay(m_kcp, 1, 20, 13, 1);
            ikcp_wndsize(m_kcp, 2048, 2048);
            m_kcp->rx_minrto = 400;
            m_kcp->interval = 1;
        }
    }

    void input(char* udp_data, size_t bytes_recved, sylar::Address::ptr peer_addr) {
        m_lastRecvPtime = sylar::GetCurrentMs();
        m_peerAddr = peer_addr; // check it TODO
        ikcp_input(m_kcp, udp_data, bytes_recved);
        {
            char kcp_buf[1024 * 1000] = "";
            int upper_recved = ikcp_recv(m_kcp, kcp_buf, sizeof(kcp_buf));
            if (upper_recved <= 0) {
                if (upper_recved == 0) {
                    SYLAR_LOG_DEBUG(g_logger) << "upper recv == 0";
                }
                // < 0 EAGAIN
            } else {
                s_count_send_kcp_packet++;
                s_count_send_kcp_size += upper_recved;
                const std::string package(kcp_buf, upper_recved);
                //SYLAR_LOG_DEBUG(g_logger) << "upper recv > 0"
                 // << " conn: " << m_kcp_id
                 // << " last_time: " << m_lastRecvPtime
                 // << " upper_recv_bytes: " << upper_recved;
                  //<< " package: " << package;
                send_msg(package);
            }
        }
    }

    void update_kcp(uint32_t clock) {
        ikcp_update(m_kcp, clock);
    }

    bool is_timeout() {
        if (m_lastRecvPtime == 0) {
            return false;
        }
        return sylar::GetCurrentMs() - m_lastRecvPtime > 10 * 1000; // 10s timeout
    }

    void do_send_msg_in_queue() {
        std::string str = "hello world";
        send_msg(str);
    }

    int udp_output(const char*buf, int len, ikcpcb* kcp) {
        int ret = m_sock->sendTo(buf, len, m_peerAddr);
        if (false) {
            SYLAR_LOG_DEBUG(g_logger) << m_serverName << " udp_output ret: " << ret << " "
            << m_peerAddr->toString() << "  "
            << std::dynamic_pointer_cast<sylar::IPv4Address>(m_peerAddr)->getPort();
        }
        return 0;
    }

    void send_msg(const std::string& msg) {
        int send_ret = ikcp_send(m_kcp, msg.c_str(), msg.size());
        if (send_ret < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "send_msg ikcp_send < 0, ret: " << send_ret;
        }
    }

    ikcpcb* getKcp() {
        return m_kcp;
    }

    void setSock(sylar::Socket::ptr sock) {
        m_sock = sock;
    }

    std::string toString() {
        std::stringstream ss;
        ss << " m_kcp_id: " << m_kcp_id
        << " peerAddr: " << m_peerAddr->toString()
        << " lastRecvTime: " << m_lastRecvPtime;
        return ss.str();
    }

private:
    //sylar::IPAddress::ptr       m_peerAddr;
    sylar::Address::ptr         m_peerAddr;
    uint32_t                    m_kcp_id;
    bool                        m_isUdp;
    std::string                 m_serverName;
    sylar::Socket::ptr          m_sock;
    sylar::SocketStream::ptr    m_sockStream;
    ikcpcb*                     m_kcp;
    uint64_t                    m_lastRecvPtime;
};

// 用来批量update所有kcp
class KcpServerSessionMgr {
public:
    typedef std::shared_ptr<KcpServerSessionMgr> ptr;

    void update_all_kcp(uint32_t clock) {
        for (auto it = m_conns.begin(); it != m_conns.end();) {
            KcpServerSession::ptr kss = it->second;
            kss->update_kcp(clock);
            if (kss->is_timeout()) { // 置位kss 超时位
                SYLAR_LOG_DEBUG(g_logger) << "KcpServerSession is timout: " << kss->toString();
                m_conns.erase(it++);
                continue;
            }
            it++;
        }
    }

    void stop_all() {
        m_conns.clear();
    }

    KcpServerSession::ptr add_new_session(const uint32_t& conv, sylar::Address::ptr peerAddr) {
        KcpServerSession::ptr kss(new KcpServerSession(peerAddr, conv));
        m_conns[conv] = kss;
        return kss;
    }

    void remove_session(const uint32_t& conv) {
        m_conns.erase(conv);
    }

    uint32_t get_new_conv() {
        static uint32_t static_cur_conv = 1000;
        static_cur_conv++;
        return static_cur_conv;
    }

    KcpServerSession::ptr find_by_conv(const uint32_t& conv) {
        auto it = m_conns.find(conv);
        if (it == m_conns.end()) {
            return nullptr;
        } else {
            return it->second;
        }
    }

private:
    std::unordered_map<uint32_t, KcpServerSession::ptr>  m_conns;

};

////////////////////////////////////////////////////////////////////
class KcpServer : public sylar::UdpServer {
public:
    typedef std::shared_ptr<KcpServer> ptr;
    typedef sylar::RWMutex RWMutexType;
    KcpServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
              sylar::IOManager* receiver = sylar::IOManager::GetThis()) :
            UdpServer(worker, receiver),
            m_kcp_mgr(new(KcpServerSessionMgr)) {
    }
    ~KcpServer() {}

    void handle_kcp_time() {
        m_kcp_mgr->update_all_kcp(sylar::GetCurrentMs());
        sylar::IOManager::GetThis()->addTimer(1,
                     std::bind(&KcpServer::handle_kcp_time, this), false); // shared_from_this
    }

protected:
    void startReceiver(sylar::Socket::ptr sock) {
        while (!isStop()) {
            std::vector<uint8_t> buf;
            buf.resize(1024 * 72);

            sylar::Address::ptr addr(new sylar::IPv4Address);
            int count = sock->recvFrom(&buf[0], buf.size(), addr);
            if (count <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "KcpServer::startReceiver UDP recv faild errno: " << errno
                                          << " strerrno: " << strerror(errno);
                return;
            }
            SYLAR_LOG_DEBUG(g_logger) << " recvFrom ret: " << count;

            if ( is_connect_pack((char*)&buf[0], count)) {
                uint32_t conv = m_kcp_mgr->get_new_conv();
                std::string send_back_msg =  make_response_conv_pack(conv);
                sock->sendTo(send_back_msg.c_str(), send_back_msg.size(), addr);
                auto kss = m_kcp_mgr->add_new_session(conv, addr);
                kss->setSock(sock);
                SYLAR_LOG_ERROR(g_logger) << "new connect id: " << conv << " " << addr->toString();
                continue;
            }

            int kcp_id = ikcp_getconv(&buf[0]);
            KcpServerSession::ptr kss = nullptr;
            auto it = m_kcp_mgr->find_by_conv(kcp_id);
            if (it == nullptr) {
                SYLAR_LOG_ERROR(g_logger) << "connect not exist id: " << kcp_id;
                continue;
            }

            it->input((char*)&buf[0], count, addr);
        }
    }

private:
    RWMutexType			                        m_mutex;
    KcpServerSessionMgr::ptr                    m_kcp_mgr;
};

static int server_udp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    KcpServerSession* kss = static_cast<KcpServerSession*>(user);
    return kss->udp_output(buf, len, kcp);
}

void static_kcp() {
    float rate = (s_count_send_kcp_size - s_count_send_kcp_size_old) * 1.0 / 10 / 1024;
    SYLAR_LOG_DEBUG(g_logger) << "sum packets: " << s_count_send_kcp_packet
      << " rate: " << rate << "KB/s";
    s_count_send_kcp_size_old = s_count_send_kcp_size;
}

void kcpServerStart() {
    sylar::Address::ptr addr = sylar::IPAddress::Create("0.0.0.0", 9527);
    KcpServer::ptr ks(new KcpServer);
    ks->bind(addr, true);
    ks->start();
    ks->handle_kcp_time();
    sylar::IOManager::GetThis()->addTimer(1000 * 10, static_kcp, true);
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(kcpServerStart);
    iom.stop();
    return 0;
}

