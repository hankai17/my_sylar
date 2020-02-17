#ifndef __SOCKET_HH__
#define __SOCKET_HH__

#include "nocopy.hh"
#include "address.hh"
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace sylar {
    class Socket : public std::enable_shared_from_this<Socket>, Nocopyable {
    public:
        typedef std::shared_ptr<Socket> ptr;
        typedef std::weak_ptr<Socket> weak_ptr;

        enum Family {
            IPv4 = AF_INET,
            IPv6 = AF_INET6
        };

        enum Type {
            TCP = SOCK_STREAM,
            UDP = SOCK_DGRAM,
            UNIX = AF_UNIX
        };

        static Socket::ptr CreateTCP(sylar::Address::ptr address);
        static Socket::ptr CreateUDP(sylar::Address::ptr address);
        static Socket::ptr CreateTCPSocket();
        static Socket::ptr CreateUDPSocket();
        static Socket::ptr CreateTCPSocket6();
        static Socket::ptr CreateUDPSocket6();
        static Socket::ptr CreateUnixTCPSocket();
        static Socket::ptr CreateUnixUDPSocket();

        Socket(int family, int type, int protocol = 0);
        ~Socket();

        int64_t getSendTimeout();
        void setSendTimeout(uint64_t v);
        int64_t getRecvTimeout();
        void setRecvTimeout(uint64_t v);

        bool getOption(int level, int option, void* result, size_t* len);
        template <typename T>
        bool getOption(int level, int option, T& result) {
            size_t len = sizeof(T);
            return getOption(level, option, &result, &len);
        }

        bool setOption(int level, int option, const void* result, size_t len);
        template <typename T>
        bool setOption(int level, int option, const T& result) {
            return setOption(level, option, &result, sizeof(T));
        }

        Socket::ptr accept();
        bool bind(const Address::ptr addr); // Why not refer
        bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
        bool listen(int backlog = SOMAXCONN);
        bool close();

        int send(const void* buffer, size_t length, int flags = 0);
        int send(iovec* buffers, size_t length, int flags = 0);
        int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);
        int sendTo(iovec* buffers, size_t length, const Address::ptr to, int flags = 0);

        int recv(void* buffer, size_t length, int flags = 0);
        int recv(iovec* buffers, size_t length, int flags = 0);
        int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
        int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

        Address::ptr getRemoteAddress();
        Address::ptr getLocalAddress();

        int getFamily() const { return m_family; }
        int getType() const { return m_type; }
        int getProtocol() const { return m_protocol; }
        bool isConnected() const { return m_isConnected; }
        bool isVaild() const;
        int getError();
        int getSocket() const { return m_sock; }

        std::ostream& dump(std::ostream& os) const;
        std::string toString() const;

        bool cancelRead();
        bool cancelWrite();
        bool cancelAccept();
        bool cancelAll();

    private:
        void initSock();
        void newSock();
        bool init(int sock);

    private:
        int             m_sock;
        int             m_family;
        int             m_type;
        int             m_protocol;
        bool            m_isConnected;
        Address::ptr    m_localAddress;
        Address::ptr    m_remoteAddress;

    };
}
#endif