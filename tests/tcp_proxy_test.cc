#include "log.hh"
#include "iomanager.hh"
#include "tcp_server.hh"
#include "socket.hh"
#include "address.hh"
#include "bytearray.hh"
#include "stream.hh"
#include "hook.hh"
#include "buffer.hh"

#include <vector>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class TcpProxy : public sylar::TcpServer {
public:
    typedef std::shared_ptr<TcpProxy> ptr;
    TcpProxy(sylar::IOManager* worker = sylar::IOManager::GetThis(),
            sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
protected:
    virtual void handleClient(sylar::Socket::ptr client);
private:

};

TcpProxy::TcpProxy(sylar::IOManager *worker, sylar::IOManager *accept_worker)
: TcpServer(worker, accept_worker) {
}

void handleClient2(sylar::Socket::ptr cli_sock) {  // SS is not tmp value
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 90);
    sylar::Socket::ptr os_sock = sylar::Socket::CreateTCP(addr);
    if(!os_sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect os failed: " << addr->toString();
        return;
    }
    sylar::SocketStream::ptr ss(new sylar::SocketStream(os_sock)); // 只有两端都关闭了 才从cw态转为fin 很明显如果链接过多就是问题
    sylar::SocketStream::ptr cs(new sylar::SocketStream(cli_sock));

    sylar::IOManager::GetThis()->schedule([cli_sock, os_sock, addr, ss]() {
        sylar::Buffer::ptr buffer(new sylar::Buffer);

        while(true) {
            int err;
            ssize_t ret = buffer->orireadFd(cli_sock->getSocket(), &err);
            if (ret <= 0) {
                SYLAR_LOG_DEBUG(g_logger) << "client close";
                break;
            }
            SYLAR_LOG_DEBUG(g_logger) << "read client ret: " << ret;
            SYLAR_LOG_DEBUG(g_logger) << "before send buffer to os, buff->readableBytes: " << buffer->readableBytes();

            ret = ss->writeFixSize(buffer->peek(), buffer->readableBytes());
            if (ret != (ssize_t)buffer->readableBytes()) {
                SYLAR_LOG_DEBUG(g_logger) << "send to server falid: " << errno
                                          << " strerrno: " << strerror(errno);
                break;
            }
            SYLAR_LOG_DEBUG(g_logger) << "after send buffer to os, ret: " << ret;
            buffer->retrieveAll();
        }

    });

    sylar::Buffer::ptr buffer(new sylar::Buffer);
    while (true) {
        int err;
        ssize_t ret = buffer->orireadFd(os_sock->getSocket(), &err);
        if (ret < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "ret <0, errno: " << errno
                                      << " strerror: " << strerror(errno);
            break;
        } else if (ret == 0) {
            SYLAR_LOG_DEBUG(g_logger) << "os close";
            break;
        }
        SYLAR_LOG_DEBUG(g_logger) << "read os ret: " << ret;
        ret = cs->writeFixSize(buffer->peek(), buffer->readableBytes());
        if (ret != (ssize_t)buffer->readableBytes()) {
            SYLAR_LOG_DEBUG(g_logger) << "send to client falid: " << errno
                                      << " strerrno: " << strerror(errno);
            break;
        }
        buffer->retrieveAll();
    }
}

void TcpProxy::handleClient(sylar::Socket::ptr cli_sock) {
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 90);
    sylar::Socket::ptr os_sock = sylar::Socket::CreateTCP(addr);
    if(!os_sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect os failed: " << addr->toString();
        return;
    }
    sylar::IOManager::GetThis()->schedule([cli_sock, os_sock, addr]() { // sock MUST ptr, IN ORDER TO 被意外释放导致fd关闭
        //if(!os_sock->connect(addr)) {
            //SYLAR_LOG_DEBUG(g_logger) << "connect os failed: " << addr->toString();
            //return;
        //}
        //SYLAR_LOG_DEBUG(g_logger) << "os_sock.isConnect: " << os_sock->isConnected();
        // 1 send cli buf to os & recv os buf send it to cli // |--->
        // 2 Is buffer should member of TcpProxy ? // no, sylar ignore all buffer

        //sylar::ByteArray::ptr ba(new sylar::ByteArray);
        //ba->clear();
        sylar::Buffer::ptr buffer(new sylar::Buffer);
        sylar::SocketStream ss(os_sock);

        while(true) {
            /*
            std::vector<iovec> iovs;
            ba->getWriteBuffers(iovs, 1024);
            int ret = cli_sock->recv(&iovs[0], iovs.size());
            if (ret == 0) {
                SYLAR_LOG_DEBUG(g_logger) << "client closed";
                break;
            } else if (ret < 0) {
                SYLAR_LOG_DEBUG(g_logger) << "client recv errno: " << errno
                                          << " strerrno: " << strerror(errno);
                break;
            }
            sylar::SocketStream ss(os_sock);
            int ret1 = ss.writeFixSize(ba, ret);
            if (ret1 != ret) {
                SYLAR_LOG_DEBUG(g_logger) << "send to server falid: " << errno
                << " strerrno: " << strerror(errno);
                break;
            }
            SYLAR_LOG_DEBUG(g_logger) << "send to os ret: " << ret1;
             */
            int err;
            ssize_t ret = buffer->orireadFd(cli_sock->getSocket(), &err);
            if (ret <= 0) {
                SYLAR_LOG_DEBUG(g_logger) << "sock: " << cli_sock->getSocket() << "  client close";
                cli_sock->close();
                break;
            }
            SYLAR_LOG_DEBUG(g_logger) << "read client ret: " << ret;
            SYLAR_LOG_DEBUG(g_logger) << "before send buffer to os, buff->readableBytes: " << buffer->readableBytes();
            ret = ss.writeFixSize(buffer->peek(), buffer->readableBytes());
            if (ret != (ssize_t)buffer->readableBytes()) {
                SYLAR_LOG_DEBUG(g_logger) << "send to server falid: " << errno
                                          << " strerrno: " << strerror(errno);
                break;
            }
            SYLAR_LOG_DEBUG(g_logger) << "after send buffer to os, ret: " << ret;
            buffer->retrieveAll();
        }

    });

    // 1 recv os buf & send to cli // --->|
    //sylar::ByteArray::ptr ba(new sylar::ByteArray);
    //ba->clear();
    sylar::Buffer::ptr buffer(new sylar::Buffer);
    sylar::SocketStream ss(cli_sock);
    while (true) {
        //if (!os_sock->isConnected()) {
            //SYLAR_LOG_DEBUG(g_logger) << "os sock is not connected";
            //continue;
        //}
        /*
        std::vector<iovec> iovs;
        ba->getWriteBuffers(iovs, 1024);
        int ret = os_sock->recv(&iovs[0], iovs.size());
        if (ret == 0) {
            SYLAR_LOG_DEBUG(g_logger) << "server closed";
            break;
        } else if (ret < 0) {
            SYLAR_LOG_DEBUG(g_logger) << "server recv errno: " << errno
                                      << " strerrno: " << strerror(errno);
            break;
        }

        sylar::SocketStream ss(cli_sock);
        if (ss.writeFixSize(ba, ret) != ret) {
            SYLAR_LOG_DEBUG(g_logger) << "send to cli falid: " << errno
                                      << " strerrno: " << strerror(errno);
            break;
        }
        ba->setPosition(0);
         */
        int err;
        ssize_t ret = buffer->orireadFd(os_sock->getSocket(), &err);
        if (ret < 0) {
            os_sock->close();
            SYLAR_LOG_DEBUG(g_logger) << "ret <0, errno: " << errno
                                      << " strerror: " << strerror(errno);
            break;
        } else if (ret == 0) {
            os_sock->close();
            SYLAR_LOG_DEBUG(g_logger) << "os close";
            break;
        }
        SYLAR_LOG_DEBUG(g_logger) << "read os ret: " << ret;
        ret = ss.writeFixSize(buffer->peek(), buffer->readableBytes());
        if (ret != (ssize_t)buffer->readableBytes()) {
            SYLAR_LOG_DEBUG(g_logger) << "send to client falid: " << errno
                                      << " strerrno: " << strerror(errno);
            break;
        }
        buffer->retrieveAll();
    }
}

void test() {
    TcpProxy::ptr tcpproxy(new TcpProxy);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 9527);
    tcpproxy->bind(addr);
    tcpproxy->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(test);
    iom.stop();
    return 0;
}