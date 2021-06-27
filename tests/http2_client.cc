#include "my_sylar/log.hh"
#include "my_sylar/address.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/stream.hh"
#include "my_sylar/fiber.hh"
#include "my_sylar/http2/http2_stream.hh"
#include <signal.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_client() {
    //sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("192.168.1.7", 80);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("192.168.0.111", 80);
    auto sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        SYLAR_LOG_ERROR(g_logger) << "connect failed, errno: " << errno << ", strerror: "
        << strerror(errno);
        return;
    }
    sylar::http2::Http2Stream::ptr stream(new sylar::http2::Http2Stream(sock, true));
    if (!stream->handleShakeClient()) {
        SYLAR_LOG_ERROR(g_logger) << "handleShakeClient failed";
        return;
    }
    stream->start();
    for (int i = 0; i < 1; i++) {
        sylar::http::HttpRequest::ptr req(new sylar::http::HttpRequest);
        req->setHeader(":method", "GET");
        req->setHeader(":scheme", "http");
        req->setHeader(":path", "/");
        req->setHeader(":authority", "0.0.0.0");

        //req->setHeader(":content-type", "text/html");
        //req->setHeader(":user-agent", "curl");
        //req->setHeader(":hello", "world");
        //req->setBody("hello world");

        auto ret = stream->request(req, 100);
        SYLAR_LOG_ERROR(g_logger) << ret->toString();
        sleep(10);
    }
    return;
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    sylar::IOManager iom(4, false, "io");
    iom.schedule(test_client);
    iom.addTimer(1000, [](){}, true);
    iom.stop();
    return 0;
}
