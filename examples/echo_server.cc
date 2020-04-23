#include "my_sylar/log.hh"
#include "my_sylar/tcp_server.hh"
#include "my_sylar/bytearray.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/buffer.hh"

#include <memory>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class EchoServer : public sylar::TcpServer {
public:
    typedef std::shared_ptr<EchoServer> ptr;
    void handleClient(sylar::Socket::ptr client) {
        SYLAR_LOG_DEBUG(g_logger) << client->toString();
        sylar::Buffer::ptr buf(new sylar::Buffer);
        int err;
        while (true) {
            int ret = buf->orireadFd(client->getSocket(), &err);
            if (ret == 0) {
                SYLAR_LOG_DEBUG(g_logger) << "peer closed";
                break;
            } else if (ret < 0) {
                SYLAR_LOG_DEBUG(g_logger) << "recv errno: " << errno
                << " strerrno: " << strerror(errno);
                break;
            }

            SYLAR_LOG_DEBUG(g_logger) << buf->peek();
            ret = buf->writeFd(client->getSocket(), ret, &err);
            if (ret == 0) {
                SYLAR_LOG_DEBUG(g_logger) << "peer closed";
                break;
            } else if (ret < 0) {
                SYLAR_LOG_DEBUG(g_logger) << "recv errno: " << errno
                << " strerrno: " << strerror(errno);
                break;
            }
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
