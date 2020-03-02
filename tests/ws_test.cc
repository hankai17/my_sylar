#include "http/ws_connection.hh"
#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include "socket.hh"
#include <map>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_ws_conn() {
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 9527);
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if(!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect failed";
        return;
    }

    //std::map<std::string, std::string> headers;
    //headers.insert(std::make_pair());
    auto p = sylar::http::WSConnection::Create("http://127.0.0.1:9527/1.c", 300);
    //return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::OK, resp, "ok"), conn);
    SYLAR_LOG_DEBUG(g_logger) << p.first->toString();
}

// websocketd --address=0.0.0.0 --port=9527 --dir=/root

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(test_ws_conn);
    iom.stop();
    return 0;
}