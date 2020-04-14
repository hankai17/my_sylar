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

std::string to_hex(const std::string& str) {
    std::stringstream ss;
    for(size_t i = 0; i < str.size(); ++i) {
        ss << std::setw(2) << std::setfill('0') << std::hex
           << (int)(uint8_t)str[i];
    }
    return ss.str();
}

void TcpProxy::handleClient(sylar::Socket::ptr client) {
    SYLAR_LOG_DEBUG(g_logger) << "start";

    sylar::Stream::ptr cs(new sylar::SocketStream(client));
    std::string buffer;
    buffer.resize(20);
    if (cs->readFixSize(&buffer[0], 2) <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "read client failed";
        return;
    }
    SYLAR_LOG_DEBUG(g_logger) << "read client: " << to_hex(buffer);
    buffer[0] = 5;
    buffer[1] = 0;
    if (cs->writeFixSize(&buffer[0], 2) <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "write client failed";
        return;
    }

    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 1984);
    sylar::Socket::ptr os_sock = sylar::Socket::CreateTCP(addr);
    if(!os_sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect os failed: " << addr->toString();
        return;
    }
    sylar::Stream::ptr ss(new sylar::SocketStream(os_sock));

    connectThem(cs, ss);
    return;
}

void test() {
    sylar::TcpServer::ptr proxy(new TcpProxy);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("0.0.0.0", 2048);
    proxy->bind(addr);
    proxy->start();
}

int main() {
    sylar::IOManager iom(1, false, "io"); 
    iom.schedule(test);
    iom.stop();
    return 0; 
}

