#include "http/ws_connection.hh"
#include "http/ws_server.hh"
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
    //auto p = sylar::http::WSConnection::Create("http://127.0.0.1:9527", 300);
    auto p = sylar::http::WSConnection::Create("http://127.0.0.1:9526/path", 3000);
    SYLAR_LOG_DEBUG(g_logger) << p.first->toString();
    if (p.second == nullptr) {
        SYLAR_LOG_DEBUG(g_logger) << "create uri failed";
        return;
    }
    // 先有一次http的recvResponse 再有recvMessage

    sylar::http::WSConnection::ptr conn = p.second;
    sylar::http::WSFrameMessage::ptr msg;
    while( (msg = conn->recvMessage()) != nullptr) { // websocked一直维护着长连接 直到我读到10后仍while继续读 就超时
        SYLAR_LOG_DEBUG(g_logger) << "recv: " << msg->getData();
    }
}

// websocketd --address=0.0.0.0 --port=9527 --dir=/root
// websocketd --address=0.0.0.0 --port=9527  ./count.sh

void test_ws_server() {
    sylar::http::WSServer::ptr server(new sylar::http::WSServer);
    sylar::IPv4Address::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 9526);
    if (!addr) {
        SYLAR_LOG_DEBUG(g_logger) << "get addresss error";
        return;;
    }
    auto fun = [](sylar::http::HttpRequest::ptr req, sylar::http::WSFrameMessage::ptr msg,
            sylar::http::WSSession::ptr session) {
        session->sendMessage(msg);
        return 0;
    };

    server->getWSServletDispatch()->addServlet("/path", fun);
    while (!server->bind(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "bind failed";
        sleep(1);
    }
    server->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(test_ws_server);
    iom.schedule(test_ws_conn);
    iom.stop();
    return 0;
}