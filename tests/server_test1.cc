#include "tcp_server.hh"
#include "log.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run() {
    auto addr = sylar::IPv4Address::Create("127.0.0.1", 9527);
    std::vector<sylar::Address::ptr> addrs;
    addrs.push_back(addr);

    sylar::TcpServer::ptr tcp_server(new sylar::TcpServer);
    std::vector<sylar::Address::ptr> fails;
    while (!tcp_server->bind(addrs, fails)) {
        sleep(2);
    }
    tcp_server->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(run);
    iom.stop();
    return 0;
}