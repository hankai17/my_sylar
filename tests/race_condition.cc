#include "http/http_parser.hh"
#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include "socket.hh"
#include "buffer.hh"

#include <fstream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_socket() {
    auto addr = sylar::IPv4Address::Create("127.0.0.1");
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    addr->setPort(9527);

    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect failed " << addr->toString()
                                  << " errno: " << strerror(errno);
        return;
    } else {
        //SYLAR_LOG_DEBUG(g_logger) << "connect success";
    }

    const char buff[] = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\n\r\n";
    int ret = sock->send(buff, strlen(buff));
    if (ret <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "send failed ret: " << ret;
        return;
    } else if (ret == (int)strlen(buff)) {
        SYLAR_LOG_DEBUG(g_logger) << " send request ok " << ret;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "send incompletable ret: " << ret;
    }

    sylar::Buffer::ptr buffer(new sylar::Buffer);
    sylar::http::HttpResponseParser parser;
    size_t header_length = 0;
    int once_flag = 1;
    int errnos;
    sock->setRecvTimeout(5000);

    while((ret = buffer->readFd(sock->getSocket(), &errnos)) > 0) {
        if (true) {
            buffer->retrieve(ret);
        } else {
            if (!parser.isFinished()) {
                header_length = parser.execute(buffer->peek(), buffer->readableBytes());
                SYLAR_LOG_DEBUG(g_logger) << "parsed: " << header_length;
            }
            if (parser.isFinished() && once_flag == 1) {
                SYLAR_LOG_DEBUG(g_logger) << "parsed finished, Content-Length: "
                                          << parser.getContentLength()
                                          << "   " << parser.getData()->getHeader("content-length", "null");
                SYLAR_LOG_DEBUG(g_logger) << parser.getData()->toString();
                std::stringstream ss;
                parser.getData()->dump(ss);
                SYLAR_LOG_DEBUG(g_logger) << ss.str();
                buffer->retrieve(header_length - 1);
                once_flag = 2;
            }
            if (parser.isFinished() && parser.getContentLength() > 0) {
                SYLAR_LOG_DEBUG(g_logger) << "parser content-length: " << parser.getContentLength()
                                          << "  buffer->readableBytes(): " << buffer->readableBytes();
                if (parser.getContentLength() == buffer->readableBytes()) {
                    break;
                }
            }
        }
    }

    if (ret == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "recv: err: " << strerror(errno);
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "recv done ret: " << ret;
    }
}

int main() {
    sylar::IOManager iom(2, false, "io");
    for (int i = 0; i < 1; i++) {
        iom.schedule(test_socket);
    }
    iom.stop();
    return 0;
}
