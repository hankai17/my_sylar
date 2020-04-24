#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/stream.hh"
#include "my_sylar/uri.hh"
#include "my_sylar/tcp_server.hh"
#include "my_sylar/http/http_connection.hh"
#include "my_sylar/ns/ares.hh"
#include "my_sylar/socks.hh"
#include "my_sylar/macro.hh"
#include <signal.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
sylar::AresChannel::ptr channel = nullptr;

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

static void shuttleData(sylar::Stream::ptr oneEnd, sylar::Stream::ptr otherEnd) {
    try {
        sylar::TransferStream(oneEnd, otherEnd);
        //uint64_t total_transfer = sylar::TransferStream(oneEnd, otherEnd);
        //int tmp_fd = std::dynamic_pointer_cast<sylar::SocketStream>(otherEnd)->getSocket()->getSocket();
        std::dynamic_pointer_cast<sylar::SocketStream>(otherEnd)->shutdown(SHUT_WR);

        //SYLAR_LOG_DEBUG(g_logger) << "oneEnd fd: "
        //<< std::dynamic_pointer_cast<sylar::SocketStream>(oneEnd)->getSocket()->getSocket()
        //<< " had closed,"
        //<< " shutdown otherEnd fd: " << tmp_fd
        //<< ", total transfered: " << total_transfer;
    } catch (std::exception &) {
        SYLAR_LOG_DEBUG(g_logger) << "shuttleData failed";
    }
}

static void connectThem(sylar::Stream::ptr oneEnd, sylar::Stream::ptr otherEnd) {
    sylar::Scheduler* scheduler = sylar::Scheduler::GetThis();
    scheduler->schedule(std::bind(&shuttleData, oneEnd, otherEnd));
    scheduler->schedule(std::bind(&shuttleData, otherEnd, oneEnd));
}

void TcpProxy::handleClient(sylar::Socket::ptr client) {
    sleep(2);
    sylar::Stream::ptr cs(new sylar::SocketStream(client));
    sylar::Stream::ptr ss = nullptr;
    if (getName() == "p1") {
        ss = tunnel(cs, nullptr, "0.0.0.0", 1967);
    } else if (getName() == "p2") {
        ss = tunnel(cs, channel);
    }
    if (ss == nullptr) {
        SYLAR_LOG_DEBUG(g_logger) << "tunnel return ss nullptr";
        return;
    } 
    //int cs_fd = std::dynamic_pointer_cast<sylar::SocketStream>(cs)->getSocket()->getSocket();
    //int ss_fd = std::dynamic_pointer_cast<sylar::SocketStream>(ss)->getSocket()->getSocket();
    //SYLAR_LOG_DEBUG(g_logger) << "start a tunnel: cs fd: " << cs_fd
      //<< ", ss fd: " << ss_fd;
    connectThem(cs, ss);
    return;
}

void test_p2() {
    sylar::TcpServer::ptr proxy(new TcpProxy);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("0.0.0.0", 1966);
    proxy->bind(addr);
    proxy->setName("p2");
    proxy->start();
}

void test_p1() {
    sylar::TcpServer::ptr proxy(new TcpProxy);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("0.0.0.0", 2048);
    proxy->bind(addr);
    proxy->setName("p1");
    proxy->start();
}

void ares_test() {
    channel.reset(new sylar::AresChannel);
    channel->init();
    channel->start();
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    sylar::IOManager iom(3, false, "io");
    //iom.schedule(ares_test);
    iom.schedule(test_p1);
    iom.schedule(test_p2);
    iom.stop();
    return 0; 
}

