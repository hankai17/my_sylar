#include "log.hh"
#include "iomanager.hh"
#include "stream.hh"
#include "uri.hh"
#include "tcp_server.hh"
#include "http/http_connection.hh"

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

static void shuttleData(sylar::Stream::ptr oneEnd, sylar::Stream::ptr otherEnd) {
    try {
        sylar::TransferStream(*oneEnd.get(), *otherEnd.get());
        /*
        if (otherEnd->supportsHalfClose())
            otherEnd->close(Stream::WRITE);
            */
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
    SYLAR_LOG_DEBUG(g_logger) << "start";
    sylar::Uri::ptr proxy = sylar::Uri::Create("http://10.0.140.173:1984");
    if (proxy == nullptr) {
        SYLAR_LOG_DEBUG(g_logger) << "proxy is null";
        return;
    }
    //sylar::IPAddress::ptr addr = sylar::IPAddress::Create("10.0.")
    std::string cli_buf;
    sylar::Stream* stream_p = tunnel(proxy, nullptr, "soft.duote.org", 80, 5, cli_buf);
    if (stream_p == nullptr) {
        SYLAR_LOG_DEBUG(g_logger) << "stream is null";
        return;
    }

    sylar::Stream::ptr ss(stream_p, [](sylar::Stream*){});
    sylar::Stream::ptr cs(new sylar::SocketStream(client));
    cs->writeFixSize(cli_buf.c_str(), cli_buf.size());
    connectThem(cs, ss);
    return;
}

void test() {
    sylar::TcpServer::ptr proxy(new TcpProxy);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("172.16.3.144", 2048);
    proxy->bind(addr);
    proxy->start();
}

int main() {
    sylar::IOManager iom(1, false, "io"); 
    iom.schedule(test);
    iom.stop();
    return 0; 
}

