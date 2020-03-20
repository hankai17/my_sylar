#include <rock/rock.hh>
#include "log.hh"
#include "iomanager.hh"
#include "tcp_server.hh"
#include "stream.hh"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class SockServer : public sylar::TcpServer {
public:
    typedef std::shared_ptr<SockServer> ptr;
    SockServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
            sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
protected:
    virtual void handleClient(sylar::Socket::ptr client) override;
    std::string     m_type;
};

SockServer::SockServer(sylar::IOManager *worker, sylar::IOManager *accept_worker)
: TcpServer(worker, accept_worker) {
    m_type = "sock";
}

void SockServer::handleClient(sylar::Socket::ptr client) {
    SYLAR_LOG_DEBUG(g_logger) << "handleClinet: " << client->toString();

    sylar::IPAddress::ptr addr = sylar::IPAddress::Create("127.0.0.1", 90);
    sylar::Socket::ptr os_sock = sylar::Socket::CreateTCP(addr);
    os_sock->connect(addr);

    sylar::RockSession::ptr session(new sylar::RockSession(os_sock));
    session->setDisConnectCb([](sylar::AsyncSocketStream::ptr sock) {
        ;
    });
    session->setRequestHandler([](sylar::RockRequest::ptr req, sylar::RockResponse::ptr resp,
            sylar::RockStream::ptr conn)->bool {
            bool ret = false;
            return ret;
    });

    session->setNotifyHandler([](sylar::RockNotify::ptr noti,
            sylar::RockStream::ptr conn)->bool {
        bool ret = false;
        return ret;
    });
    session->start();
}

void test() {
    SockServer::ptr server(new SockServer);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 9527);
    while (!server->bind(addr)) {
        sleep(2);
    }
    server->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(test);
    iom.stop();
    return 0;
}