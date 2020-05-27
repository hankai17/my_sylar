#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/stream.hh"
#include "my_sylar/tcp_server.hh"
#include "my_sylar/thread.hh"
#include "my_sylar/address.hh"
#include "my_sylar/util.hh"
#include "my_sylar/nocopy.hh"
extern "C" {
#include "my_sylar/utils/ikcp.h"
}
#include <iostream>
#include <memory>
#include <vector>
#include <atomic>

#include "kcp_utils.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
static std::atomic<uint64_t>    s_count_send_kcp_packet;
static std::atomic<uint64_t>    s_count_send_kcp_size;
uint64_t    s_count_send_kcp_size_old;

class KcpClientSession;

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

class KcpClientSession : public std::enable_shared_from_this<KcpClientSession>,
        sylar::Nocopyable {
public:
    typedef std::shared_ptr<KcpClientSession> ptr;
    KcpClientSession(sylar::IPAddress::ptr peerAddr, bool is_udp = true,
                     const std::string& client_name = "p1") :
            m_peerAddr(peerAddr),
            m_isUdp(is_udp),
            m_serverName(client_name),
            m_isConnected(false) {
        m_sock = is_udp ? sylar::Socket::CreateUDPSocket() : nullptr; // TCP TODO
        int sndbuf = 65536;
        if (!m_sock->setOption(SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int)) ) {
            SYLAR_LOG_DEBUG(g_logger) << "setOption failed errno: " << errno
              << " strerrno: " << strerror(errno);
        }
        m_sockStream.reset(new sylar::SocketStream(m_sock));
    }

    ~KcpClientSession() {
        if (m_kcp) {
            ikcp_release(m_kcp);
        }
    }

    void do_send_connect_pack() {
        std::string pack = make_connect_pack();
        const ssize_t ret = m_sockStream->getSocket()->sendTo(pack.c_str(), pack.length(), m_peerAddr);
        if (ret < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "do_send_connect_pack falied ret: " << ret
            << " errno: " << errno << " strerror: " << strerror(errno);
        }
        return;
    }

    void do_recv_connect_back_pack() {
        char recv_buf[1400] = "";
        sylar::Address::ptr addr(new sylar::IPv4Address);
        set_recv_timeout(1000);
        const ssize_t ret = m_sockStream->getSocket()->recvFrom(recv_buf, sizeof(recv_buf), addr); // peerAddr check TODO
        if (ret <= 0) {
            if (ret == 0) {
                SYLAR_LOG_DEBUG(g_logger) << "do_recv_connect_back_pack falied ret: = 0 ";
            }
            SYLAR_LOG_DEBUG(g_logger) << "do_recv_connect_back_pack falied ret: " << ret
                                      << " errno: " << errno << " strerror: " << strerror(errno);
            return;
        }
        if (!is_response_conv_pack(recv_buf, ret)) {
            SYLAR_LOG_DEBUG(g_logger) << "do_recv_connect_back_pack falied not ideal package";
            return;
        }
        uint32_t id = parse_conv_from_response_conv_pack(recv_buf, ret);
        //SYLAR_LOG_DEBUG(g_logger) << "get id: " << id;
        init_kcp(id);
    }

    void init_kcp(uint32_t id) {
        m_kcp_id = id;
        m_kcp = ikcp_create(m_kcp_id, this); // same as httpparser
        m_kcp->output = m_isUdp ? &KcpClientSession::upper_udp_output : nullptr; // TCP TODO
        if (true) {
            m_kcp->interval = 1;
            m_kcp->rx_minrto = 2000;
            ikcp_wndsize(m_kcp, 2048, 2048); // wndsize is important
            //ikcp_wndsize(m_kcp, 20480, 20480); // wndsize is important
            ikcp_nodelay(m_kcp, 1, 5, 2, 1); // para2 is important  70MB/s & mem increase slow
        } else { // 40MB/s
            ikcp_nodelay(m_kcp, 1, 20, 13, 1);
            ikcp_wndsize(m_kcp, 2048, 2048);
            m_kcp->rx_minrto = 400;
            m_kcp->interval = 1;
        }
        m_isConnected = true;
    }

    void do_send_msg_in_queue() {
        std::string str = "hello world";
        send_msg(str);
    }

    void set_recv_timeout(uint64_t recv_timeout) {
        //setsockopt(m_sockStream->getSocket()->getSocket(), SOL_SOCKET, SO_RCVTIMEO, (char*)&recv_timeout, sizeof(int));
        m_sockStream->getSocket()->setRecvTimeout(recv_timeout);
    }

    void do_recv_udp_in_loop() {
        while (1) {
            if (!m_isConnected) {
                break;
            }
            std::vector<uint8_t> buf;
            buf.resize(20 * 1024);

            set_recv_timeout(1000 * 10);
            int count = m_sockStream->getSocket()->recvFrom(&buf[0], buf.size(), m_peerAddr); // Why lower recv once However upper recv once more ?
            if (count <= 0) {
                if (count == 0) {
                    SYLAR_LOG_DEBUG(g_logger) << "do_recv_udp_in_loop ret == 0";
                    return;
                }
                SYLAR_LOG_DEBUG(g_logger) << "do_recv_udp_in_loop ret < 0"
                                          << " errno: " << errno << " strerrno: " << strerror(errno);
                m_isConnected = false;
                return;
            }
            SYLAR_LOG_DEBUG(g_logger) << "do_recv_udp_in_loop ret: " << count;

            // check peerAddr TODO
            if (is_disconnect_pack((char*)&buf[0], count)) {
                m_isConnected = false;
            }

            ikcp_input(m_kcp, (char*)&buf[0], count);

            while (1) {
                char* buf = nullptr;
                uint32_t len = 0;
                int ret = recv_kcp(buf, len, m_kcp);
                if (ret <= 0) {
                    //SYLAR_LOG_DEBUG(g_logger) << "upper_recv ret: " << ret;
                    break;
                } else {
                    SYLAR_LOG_DEBUG(g_logger) << "client upper recv bufsize: " << len;
                }
            }
        }
    }

    void stop() {
        // How to do
    }

    void update() { // Must connected before call this func
        if (!m_isConnected) {
            SYLAR_LOG_DEBUG(g_logger) << "update: can not connect server...";
            stop();
            return;
        }
        do_send_msg_in_queue();
        //do_recv_udp_in_loop();
        ikcp_update(m_kcp, sylar::GetCurrentMs());

        sylar::IOManager::GetThis()->addTimer(5,
                std::bind(&KcpClientSession::update, shared_from_this()), false);
    }

    static int upper_udp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
        KcpClientSession* kcs = static_cast<KcpClientSession*>(user);
        return kcs->udp_output(buf, len, kcs->getKcp());
    }

    int udp_output(const char* buf, int len, ikcpcb* kcp) {
        int ret = m_sockStream->getSocket()->sendTo(buf, len, m_peerAddr);
        s_count_send_kcp_size += ret;
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

    ikcpcb* getKcp() { return m_kcp; }
    bool isConnect() { return m_isConnected; }
    uint32_t getKcpId() { return m_kcp_id; }

private:
    sylar::IPAddress::ptr       m_peerAddr;
    bool                        m_isUdp;
    std::string                 m_serverName;
    sylar::Socket::ptr          m_sock;
    sylar::SocketStream::ptr    m_sockStream;
    ikcpcb*                     m_kcp;
    uint32_t                    m_kcp_id;
    bool                        m_isConnected;
};

void static_kcp() {
    float rate = (s_count_send_kcp_size - s_count_send_kcp_size_old) * 1.0 / 10 / 1024;
    SYLAR_LOG_DEBUG(g_logger) << "sum packets: " << s_count_send_kcp_packet
      << " rate: " << rate << "KB/s";
    s_count_send_kcp_size_old = s_count_send_kcp_size;
}

void p1_test() {
    //KcpClientSession::ptr kcs(new KcpClientSession(sylar::IPAddress::Create("172.16.3.98", 9527)));
    KcpClientSession::ptr kcs(new KcpClientSession(sylar::IPAddress::Create("0.0.0.0", 9527)));

    sylar::IOManager::GetThis()->addTimer(1000 * 10, static_kcp, true);
    kcs->do_send_connect_pack();
    kcs->do_recv_connect_back_pack();
    if (!kcs->isConnect()) {
        SYLAR_LOG_DEBUG(g_logger) << "Can not connect server";
        return;
    }

    SYLAR_LOG_DEBUG(g_logger) << "Connect success id: " << kcs->getKcpId();
    sylar::IOManager::GetThis()->schedule(std::bind(&KcpClientSession::do_recv_udp_in_loop, kcs));
    sylar::IOManager::GetThis()->addTimer(5,
        std::bind(&KcpClientSession::update, kcs), false);
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(p1_test);
    iom.stop();
    return 0;
}

