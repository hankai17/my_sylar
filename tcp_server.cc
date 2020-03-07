#include "tcp_server.hh"
#include "log.hh"
#include "config.hh"

namespace sylar {

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static sylar::ConfigVar<uint64_t >::ptr g_tcp_server_read_timeout =
            sylar::Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2),
                    "tcp server read timeout");

    TcpServer::TcpServer(IOManager* worker,
            IOManager* accept_worker)
            : m_worker(worker),
            m_acceptWorker(accept_worker),
            m_recvTimeout(g_tcp_server_read_timeout->getValue()),
            m_name("sylar/1.0.0"),
            m_isStop(true) {
    }

    TcpServer::~TcpServer() {
        for (const auto& i : m_socks) {
            i->close();
        }
        m_socks.clear();
    }

    bool TcpServer::bind(const std::vector<Address::ptr> &addrs,
                         std::vector<Address::ptr> &fails, bool ssl) {
        for (const auto& i : addrs) {
            Socket::ptr sock = ssl ? SSLSocket::CreateTCP(i) :Socket::CreateTCP(i);
            if (!sock->bind(i)) {
                SYLAR_LOG_ERROR(g_logger) << "bind failed " << i->toString()
                << " errno: " << errno
                << " strerrno: " << strerror(errno);
                fails.push_back(i);
                continue;
            }
            if (!sock->listen()) {
                SYLAR_LOG_ERROR(g_logger) << "listen failed " << i->toString()
                << " errno: " << errno
                << " strerrno: " << strerror(errno);
                fails.push_back(i);
                continue;
            }
            m_socks.push_back(sock);
        }
        if (!fails.empty()) {
            m_socks.clear();
            return false;
        }

        for (const auto& i : m_socks) {
            SYLAR_LOG_DEBUG(g_logger) << "server bind success: " << i->toString();
        }
        return true;
    }

    bool TcpServer::bind(Address::ptr addr, bool ssl) {
        std::vector<Address::ptr> addrs;
        std::vector<Address::ptr> fails;
        addrs.push_back(addr);
        return bind(addrs, fails, ssl);
    }

    void TcpServer::startAccept(Socket::ptr sock) {
        while (!m_isStop) {
            Socket::ptr client = sock->accept();
            if (client) {
                client->setRecvTimeout(m_recvTimeout);
                m_worker->schedule(std::bind(&TcpServer::handleClient,
                        shared_from_this(),
                        client));
            } else {
                SYLAR_LOG_ERROR(g_logger) << "accept errno: " << errno
                << " strerrno: " << strerror(errno);
            }
        }
    }

    bool TcpServer::start() {
        if (!m_isStop) {
            return true;
        }
        m_isStop = false;
        for (const auto& i : m_socks) {
            m_acceptWorker->schedule(std::bind(&TcpServer::startAccept,
                    shared_from_this(), i));
        }
        return true;
    }

    void TcpServer::stop() {
        m_isStop = true;
        auto self = shared_from_this();
        m_acceptWorker->schedule([this, self](){
            for (const auto& i : m_socks) {
                i->cancelAll();
                i->close();
            }
            m_socks.clear();
        });
    }

    void TcpServer::handleClient(Socket::ptr client) {
        SYLAR_LOG_DEBUG(g_logger) << "handleClient: " << client->toString();
    }

    bool TcpServer::loadCertificates(const std::string& cert_file, const std::string& key_file) {
        for (const auto& i : m_socks) {
            auto ssl_socket = std::dynamic_pointer_cast<SSLSocket>(i);
            if (ssl_socket) {
                if (!ssl_socket->loadCertificates(cert_file, key_file)) {
                    return false;
                }
            }
        }
        return true;
    }

}
