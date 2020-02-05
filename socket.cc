#include "socket.hh"
#include "log.hh"
#include "fd_manager.hh"
#include "macro.hh"

#include <sys/socket.h>

namespace sylar {
static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    Socket::Socket(int family, int type, int protocol)
        : m_sock(-1),
        m_family(family),
        m_type(type),
        m_protocol(protocol),
        m_isConnected(false) {
    }

    Socket::~Socket() {
        close();
    }

    bool Socket::close() {
        if (!m_isConnected && m_sock == -1) {
            return true;
        }
        m_isConnected = false;
        if (m_sock == -1) {
            ::close(m_sock);
            m_sock = -1;
        }
        return false; // ???
    }

    Socket::ptr Socket::CreateTCP(sylar::Address::ptr address) {
        Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDP(sylar::Address::ptr address) {
        Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateTCPSocket() {
        Socket::ptr sock(new Socket(IPv4, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDPSocket() {
        Socket::ptr sock(new Socket(IPv4, UDP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateTCPSocket6() {
        Socket::ptr sock(new Socket(IPv6, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDPSocket6() {
        Socket::ptr sock(new Socket(IPv6, UDP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUnixTCPSocket() {
        Socket::ptr sock(new Socket(UNIX, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUnixUDPSocket() {
        Socket::ptr sock(new Socket(UNIX, UDP, 0));
        return sock;
    }

    int64_t Socket::getSendTimeout() {
        FdCtx::ptr ctx = FdManager::getFdMgr()->get(m_sock);
        if (ctx) {
            return ctx->getTimeout(SO_SNDTIMEO);
        }
        return -1;
    }

    void Socket::setSendTimeout(uint64_t v) {
        struct timeval tv { int(v / 1000), int(v % 1000 / 1000) };
        setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
    }

    int64_t Socket::getRecvTimeout() {
        FdCtx::ptr ctx = FdManager::getFdMgr()->get(m_sock);
        if (ctx) {
            return ctx->getTimeout(SO_RCVTIMEO);
        }
        return -1;
    }

    void Socket::setRecvTimeout(uint64_t v) {
        struct timeval tv { int(v / 1000), int(v % 1000 / 1000) };
        setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
    }

    bool Socket::getOption(int level, int option, void* result, size_t* len) {
        int ret = getsockopt(m_sock, level, option, result, (socklen_t*)len);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::getOption(" << m_sock << ", " << level << ", "
            << option << "...) err: " <<  ret
            << " errno: " << errno << " strerror: " << strerror(errno);
            return false;
        }
        return true;
    }

    bool Socket::setOption(int level, int option, const void* result, size_t len) {
        int ret = setsockopt(m_sock, level, option, result, (socklen_t)len);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::setOption(" << m_sock << ", " << level << ", "
                                      << option << "...) err: " <<  ret
                                      << " errno: " << errno << " strerror: " << strerror(errno);
            return false;
        }
        return true;
    }

    void Socket::initSock() {
        int val = 1;
        setOption(SOL_SOCKET, SO_REUSEADDR, val);
        if (m_type == SOCK_STREAM) {
            setOption(IPPROTO_TCP, TCP_NODELAY, val);
        }
    }

    Address::ptr Socket::getLocalAddress() {
        if (m_localAddress) {
            return m_localAddress;
        }

        Address::ptr result;
        switch (m_family) {
            case AF_INET: {
                result.reset(new IPv4Address());
                break;
            }
            case AF_INET6: {
                result.reset(new IPv6Address());
                break;
            }
            case AF_UNIX: {
                result.reset(); // TODO
                break;
            }
            default: {
                result.reset(new UnknownAddress(m_family));
                break;
            }
        }
        socklen_t addrlen = result->getAddrLen();
        if (getsockname(m_sock, result->getAddr(), &addrlen)) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::getLocalAddress getsockname err: "
            << errno << " strerrno: " << strerror(errno);
            return Address::ptr(new UnknownAddress(m_family));
        }
        if (m_family == AF_UNIX) {
            // TODO
        }
        m_localAddress = result;
        return m_localAddress;
    }

    Address::ptr Socket::getRemoteAddress() {
        if (m_remoteAddress) {
            return m_remoteAddress;
        }

        Address::ptr result;
        switch (m_family) {
            case AF_INET: {
                result.reset(new IPv4Address());
                break;
            }
            case AF_INET6: {
                result.reset(new IPv6Address());
                break;
            }
            case AF_UNIX: {
                result.reset(); // TODO
                break;
            }
            default: {
                result.reset(new UnknownAddress(m_family));
                break;
            }
        }
        socklen_t addrlen = result->getAddrLen();
        if (getpeername(m_sock, result->getAddr(), &addrlen)) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::getRemoteAddress getpeername err: "
                                      << errno << " strerrno: " << strerror(errno);
            return Address::ptr(new UnknownAddress(m_family));
        }
        if (m_family == AF_UNIX) {
            // TODO
        }
        m_remoteAddress = result;
    }

    bool Socket::init(int sock) {
        FdCtx::ptr ctx = FdManager::getFdMgr()->get(sock);
        if (ctx && ctx->isSocket() && !ctx->isClosed()) {
            m_sock = sock;
            m_isConnected = true;
            initSock();
            getLocalAddress();
            getRemoteAddress();
            return true;
        }
        return false;
    }

    Socket::ptr Socket::accept() {
        Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
        int newsock = ::accept(m_sock, nullptr, nullptr);
        if (newsock == -1) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::accept(" << m_sock
            << " ...) errno: " << errno << " strerrno: " << strerror(errno);
            return nullptr;
        }
        if (sock->init(newsock)) {
            return sock;
        }
        return nullptr;
    }

    bool Socket::isVaild() {
        return m_sock != -1;
    }

    bool Socket::bind(const Address::ptr addr) { // Why not refer
        if (!isVaild()) {
            newSock();
            if (SYLAR_UNLICKLY(!isValid())) {
                return true;
            }
        }
        if (SYLAR_UNLICKLY(addr->getFamily() != m_family)) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::bind errno: " << errno
            << " strerrno: " << strerror(errno);
            return false;
        }
        getLocalAddress();
        return true;
    }

    bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
        if (!isVaild()) {
            newSock();
            if (SYLAR_UNLICKLY(!isVaild())) {
                return false;
            }
        }
        if (SYLAR_UNLICKLY(addr->getFamily() != m_family)) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::connect1 errno: " << errno
                                      << " strerrno: " << strerror(errno);
            return false;
        }
        if (timeout_ms == (unsigned int)-1) {
            if (::connect(m_sock, addr->getAddr(), addr->getAddrLen())) {
                SYLAR_LOG_ERROR(g_logger) << "Socket::connect " << addr->toString() << " errno: " << errno
                                          << " strerrno: " << strerror(errno);
                close();
                return false;
            }
        }
        m_isConnected = true;
        getRemoteAddress();
        getLocalAddress();
        return true;
    }

    bool Socket::listen(int backlog) {
        if (!isVaild()) {
            SYLAR_LOG_ERROR(g_logger) << "listen error sock=-1";
            return false;
        }
        if (::listen(m_sock, backlog)) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::listen errno: " << errno
                                      << " strerrno: " << strerror(errno);
            return false;
        }
        return true;
    }
}