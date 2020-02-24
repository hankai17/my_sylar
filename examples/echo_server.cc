#include "log.hh"
#include "tcp_server.hh"
#include "bytearray.hh"
#include "iomanager.hh"

#include <memory>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class EchoServer : public sylar::TcpServer {
public:
    typedef std::shared_ptr<EchoServer> ptr;
    void handleClient(sylar::Socket::ptr client) {
        SYLAR_LOG_DEBUG(g_logger) << client->toString();
        sylar::ByteArray::ptr ba(new sylar::ByteArray);
        while (true) {
            ba->clear();
            std::vector<iovec> iovs;
            ba->getWriteBuffers(iovs, 1024);

            int ret = client->recv(&iovs[0], iovs.size());
            if (ret == 0) {
                SYLAR_LOG_DEBUG(g_logger) << "peer closed";
                break;
            } else if (ret < 0) {
                SYLAR_LOG_DEBUG(g_logger) << "recv errno: " << errno
                << " strerrno: " << strerror(errno);
                break;
            }

            SYLAR_LOG_DEBUG(g_logger) << "ret: " << ret;
            SYLAR_LOG_DEBUG(g_logger) << ba->getPosition();
            SYLAR_LOG_DEBUG(g_logger) << ba->getReadSize();
            SYLAR_LOG_DEBUG(g_logger) << ba->getSize();
            ba->setPosition(ba->getPosition() + ret);
            SYLAR_LOG_DEBUG(g_logger) << ba->getPosition();
            SYLAR_LOG_DEBUG(g_logger) << ba->getReadSize();
            SYLAR_LOG_DEBUG(g_logger) << ba->getSize();
            //SYLAR_LOG_DEBUG(g_logger) << ba->toHexString();
            ba->setPosition(0);

            //SYLAR_LOG_DEBUG(g_logger) << "recv ret: " << ret << " data: " << std::string((char*)iovs[0].iov_base, ret);
            SYLAR_LOG_DEBUG(g_logger) << ba->toString();
            //SYLAR_LOG_DEBUG(g_logger) << ba->toHexString();
            break;
        }
    }
};

void run() {
    SYLAR_LOG_DEBUG(g_logger) << "run";
    EchoServer::ptr es(new EchoServer);
    auto addr = sylar::IPAddress::Create("127.0.0.1", 9527);
    while (!es->bind(addr)) {
        sleep(2);
    }
    es->start();
}

int main() {
    sylar::IOManager iom(2, false, "EchoServer");
    iom.schedule(run);
    iom.stop();
    return 0;
}