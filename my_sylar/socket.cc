#include "socket.hh"
#include "log.hh"
#include "fd_manager.hh"
#include "macro.hh"
#include "iomanager.hh"
#include "hook.hh"

#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <limits.h>

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
        if (m_sock != -1) {
            ::Close(m_sock);
            m_sock = -1;
        }
        return false; // ???
    }

    bool Socket::shutdown(int how) {
        if (!m_isConnected && m_sock == -1) {
            return true;
        }
        if (m_sock != -1) {
            ::Shutdown(m_sock, how);
        }
        return false;
    }

    Socket::ptr Socket::CreateTCP(sylar::Address::ptr address) {
        Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDP(sylar::Address::ptr address) {
        Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
        sock->newSock();
        sock->m_isConnected = true;
        return sock;
    }

    Socket::ptr Socket::CreateTCPSocket() {
        Socket::ptr sock(new Socket(IPv4, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDPSocket() {
        Socket::ptr sock(new Socket(IPv4, UDP, 0));
        sock->newSock();
        sock->m_isConnected = true;
        return sock;
    }

    Socket::ptr Socket::CreateTCPSocket6() {
        Socket::ptr sock(new Socket(IPv6, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDPSocket6() {
        Socket::ptr sock(new Socket(IPv6, UDP, 0));
        sock->newSock();
        sock->m_isConnected = true;
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

    void Socket::setRecvTimeout(uint64_t v) { // in ms
        struct timeval tv { int(v / 1000), int(v % 1000 * 1000) }; // para2 is us
        setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
    }

    bool Socket::getOption(int level, int option, void* result, size_t* len) {
        int ret = Getsockopt(m_sock, level, option, result, (socklen_t*)len);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::getOption(" << m_sock << ", " << level << ", "
            << option << "...) err: " <<  ret
            << " errno: " << errno << " strerror: " << strerror(errno);
            return false;
        }
        return true;
    }

    bool Socket::setOption(int level, int option, const void* result, size_t len) {
        int ret = Setsockopt(m_sock, level, option, result, (socklen_t)len);
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
        if (getsockname(m_sock, (sockaddr*)result->getAddr(), &addrlen)) {
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

    Address::ptr Socket::getRemoteAddress() { // 通常是accept后拿到的链接
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
        if (getpeername(m_sock, (sockaddr*)result->getAddr(), &addrlen)) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::getRemoteAddress getpeername err: "
                                      << errno << " strerrno: " << strerror(errno);
            return Address::ptr(new UnknownAddress(m_family));
        }
        if (m_family == AF_UNIX) {
            // TODO
        }
        m_remoteAddress = result;
        return m_remoteAddress;
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
        int newsock = ::Accept(m_sock, nullptr, nullptr);
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

    bool Socket::isVaild() const {
        return m_sock != -1;
    }

    void Socket::newSock() {
        m_sock = ::Socket(m_family, m_type, m_protocol);
        if (SYLAR_UNLICKLY(m_sock != -1)) {
            initSock();
        } else {
            SYLAR_LOG_ERROR(g_logger) << "Socket::newSock(" << m_family
            << ", " << m_type << ", " << m_protocol << ") " << " err: " << errno
            << " strerror: " << strerror(errno);
        }
    }

    bool Socket::bind(const Address::ptr addr) { // Why not refer
        if (!isVaild()) {
            newSock();
            if (SYLAR_UNLICKLY(!isVaild())) {
                return false;
            }
        }
        if (SYLAR_UNLICKLY(addr->getFamily() != m_family)) {
            SYLAR_LOG_ERROR(g_logger) << "Socket::bind errno: " << errno
            << " strerrno: " << strerror(errno);
            return false;
        }
        if (::bind(m_sock, addr->getAddr(), addr->getAddrLen())) {
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
        if (timeout_ms == (uint64_t)-1) {
            if (::Connect(m_sock, addr->getAddr(), addr->getAddrLen())) {
                SYLAR_LOG_ERROR(g_logger) << "Socket::connect " << addr->toString()
                << " errno: " << errno << " strerrno: " << strerror(errno);
                close();
                return false;
            }
        } else {
            if (::connect_with_timeout(m_sock, addr->getAddr(), addr->getAddrLen(), timeout_ms)) {
                SYLAR_LOG_ERROR(g_logger) << "Socket::connect_with_timeout " << addr->toString()
                << " timeout_ms: " << timeout_ms << " errno: " << errno << " strerrno: " << strerror(errno);
                close();
                return false;
            }
        }
        m_isConnected = true;
        getRemoteAddress();
        getLocalAddress();
        return true;
    }

    bool Socket::reconnect(uint64_t timeout_ms) {
        if (!m_localAddress) {
            SYLAR_LOG_ERROR(g_logger) << "localAddress is null";
            return false;
        }
        return connect(m_localAddress, timeout_ms);
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

    int Socket::send(const void* buffer, size_t length, int flags) {
        if (isConnected()) {
            return ::Send(m_sock, buffer, length, flags);
        }
        return -1;
    }

    int Socket::send(iovec* buffers, size_t length, int flags) {
        if (isConnected()) {
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            return ::Sendmsg(m_sock, &msg, flags);
        }
        return -1;
    }

    int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
        if (isConnected()) {
            return ::Sendto(m_sock, buffer, length, flags, to->getAddr(), to->getAddrLen());
        }
        return -1;
    }

    int Socket::sendTo(iovec* buffers, size_t length, const Address::ptr to, int flags) {
        if (isConnected()) {
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            msg.msg_name = (void*)to->getAddr();
            msg.msg_namelen = to->getAddrLen();
            return ::Sendmsg(m_sock, &msg, flags); // ???
        }
        return -1;
    }

    int Socket::recv(void* buffer, size_t length, int flags) {
        if (isConnected()) {
            return ::Recv(m_sock, buffer, length, flags);
        }
        return -1;
    }

    int Socket::recv(iovec* buffers, size_t length, int flags) {
        if (isConnected()) {
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            return ::Recvmsg(m_sock, &msg, flags);
        }
        return -1;
    }

    int Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
        if (isConnected()) {
            socklen_t len = from->getAddrLen();
            return ::Recvfrom(m_sock, buffer, length, flags, (sockaddr*)from->getAddr(), &len);
        }
        return -1;
    }

    int Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
        if (isConnected()) {
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            msg.msg_name = (void*)from->getAddr();
            msg.msg_namelen = from->getAddrLen();
            return ::Recvmsg(m_sock, &msg, flags);
        }
        return -1;
    }

    int Socket::getError() {
        int error = 0;
        size_t len = sizeof(error);
        if (!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
            return -1;
        }
        return error;
    }

    std::ostream& Socket::dump(std::ostream& os) const {
        os << "[Socket socket: " << m_sock
        << " is_connected: " << m_isConnected
        << " family: " << m_family
        << " type: " << m_type
        << " protocol: " << m_protocol;
        if (m_localAddress) {
            os << " local_addr: " << m_localAddress->toString();
        }
        if (m_remoteAddress) {
            os << " remote_addr: " << m_remoteAddress->toString();
        }
        os << "]";
        return os;
    }

    std::string Socket::toString() const {
        std::stringstream ss;
        dump(ss);
        return ss.str();
    }

    bool Socket::cancelRead() {
        return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::READ);
    }

    bool Socket::cancelWrite() {
        return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::WRITE);
    }

    bool Socket::cancelAccept() {
        return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::READ);
    }

    bool Socket::cancelAll() {
        return IOManager::GetThis()->cancelAll(m_sock);
    }

    namespace {
        struct _SSLInit {
            _SSLInit() {
                SSL_library_init();
                SSL_load_error_strings();
                OpenSSL_add_all_algorithms();
            }
        };
        static _SSLInit s_init;
    }

    SSLSocket::SSLSocket(int family, int type, int protocol)
    : Socket(family, type, protocol) {
    }

    bool SSLSocket::loadCertificates(const std::string& cert_file, const std::string& key_file) {
        m_ctx.reset(SSL_CTX_new(SSLv23_server_method()), SSL_CTX_free);
        if (SSL_CTX_use_certificate_chain_file(m_ctx.get(), cert_file.c_str()) != 1) {
            SYLAR_LOG_ERROR(g_logger) << "SSL_CTX_use_certificate_chain_file failed, cert_file: "
                << cert_file;
            return false;
        }
        if (SSL_CTX_use_PrivateKey_file(m_ctx.get(), key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
            SYLAR_LOG_ERROR(g_logger) << "SSL_CTX_use_PrivateKey_file failed, key_file: "
                << key_file;
            return false;
        }
        if (SSL_CTX_check_private_key(m_ctx.get()) != 1) {
            SYLAR_LOG_ERROR(g_logger) << "SSL_CTX_check_private_key failed, cert_file: " << cert_file
            << " key_file: " << key_file;
            return false;
        }
        return true;
    }

    bool SSLSocket::init(int sock) {
        bool v = Socket::init(sock);
        if (v) {
            m_ssl.reset(SSL_new(m_ctx.get()), SSL_free);
            SSL_set_fd(m_ssl.get(), m_sock);
            v = (SSL_accept(m_ssl.get()) == 1);
        }
        return v;
    }

    SSLSocket::ptr SSLSocket::CreateTCP(sylar::Address::ptr addr) {
        SSLSocket::ptr sock(new SSLSocket(addr->getFamily(), TCP, 0));
        return sock;
    }

    SSLSocket::ptr SSLSocket::CreateTCPSocket() {
        SSLSocket::ptr sock(new SSLSocket(IPv4, TCP, 0));
        return sock;
    }

    SSLSocket::ptr SSLSocket::CreateTCPSocket6() {
        SSLSocket::ptr sock(new SSLSocket(IPv6, TCP, 0));
        return sock;
    }

    Socket::ptr SSLSocket::accept() {
        SSLSocket::ptr sock(new SSLSocket(m_family, m_type, m_protocol));
        int newsock = ::Accept(m_sock, nullptr, nullptr);
        if (newsock == -1) {
            SYLAR_LOG_ERROR(g_logger) << "SSLSocket::accept(" << m_sock
                                      << " ...) errno: " << errno << " strerrno: " << strerror(errno);
            return nullptr;
        }
        sock->m_ctx = m_ctx;
        if (sock->init(newsock)) {
            return sock;
        }
        return nullptr;
    }

    bool SSLSocket::bind(const Address::ptr addr) {
        return Socket::bind(addr);
    }

    bool SSLSocket::connect(const Address::ptr addr, uint64_t timeout_ms) {
        bool v = Socket::connect(addr, timeout_ms);
        if (v) {
            m_ctx.reset(SSL_CTX_new(SSLv23_client_method()), SSL_CTX_free);
            m_ssl.reset(SSL_new(m_ctx.get()), SSL_free);
            SSL_set_fd(m_ssl.get(), m_sock);
            v = (SSL_connect(m_ssl.get()) == 1);
        }
        return v;
    }

    bool SSLSocket::listen(int backlog) {
        return Socket::listen(backlog);
    }

    bool SSLSocket::close() {
        return Socket::close();
    }

    int SSLSocket::send(const void* buffer, size_t length, int flags) {
        if (m_ssl) {
            return SSL_write(m_ssl.get(), buffer, length);
        }
        return -1;
    }

    int SSLSocket::send(iovec* buffers, size_t length, int flags) {
        if (!m_ssl) {
            return -1;
        }
        int total = 0;
        for (size_t i = 0; i < length; i++) {
            int tmp = SSL_write(m_ssl.get(), buffers[i].iov_base, buffers[i].iov_len);
            if (tmp <= 0) {
                return tmp;
            }
            total += tmp;
            if (tmp != (int)buffers[i].iov_len) {
                break;
            }
        }
        return total;
    }

    int SSLSocket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
        SYLAR_ASSERT(false);
        return -1;
    }

    int SSLSocket::sendTo(iovec* buffers, size_t length, const Address::ptr to, int flags) {
        SYLAR_ASSERT(false);
        return -1;
    }

    int SSLSocket::recv(void* buffer, size_t length, int flags) {
        if (m_ssl) {
            return SSL_read(m_ssl.get(), buffer, length);
        }
        return -1;
    }

    int SSLSocket::recv(iovec* buffers, size_t length, int flags) {
        if (!m_ssl) {
            return -1;
        }
        int total = 0;
        for (size_t i = 0; i < length; i++) {
            int tmp = SSL_read(m_ssl.get(), buffers[i].iov_base, buffers[i].iov_len);
            if (tmp <= 0) {
                return tmp;
            }
            total += tmp;
            if (tmp != (int)buffers[i].iov_len) {
                break;
            }
        }
        return total;
    }

    int SSLSocket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
        SYLAR_ASSERT(false);
        return -1;
    }

    int SSLSocket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
        SYLAR_ASSERT(false);
        return -1;
    }

    std::ostream& SSLSocket::dump(std::ostream& os) const {
        os << "[SSLSocket sock: " << m_sock
        << " is_connect: " << m_isConnected
        << " family: " << m_family
        << " type: " << m_type
        << " protocol: " << m_protocol;

        if (m_localAddress) {
            os << "localAddress: " << m_localAddress->toString();
        }
        if (m_remoteAddress) {
            os << "remoteAddress: " << m_remoteAddress->toString();
        }
        os << "]";
        return os;
    }

}