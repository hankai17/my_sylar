#include "http/http_parser.hh"
#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include "socket.hh"
#include "buffer.hh"

#include <fstream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_socket() {
    std::vector<sylar::Address::ptr> addrs;
    //sylar::Address::Lookup(addrs, "www.ifeng.com");
    sylar::Address::Lookup(addrs, "www.baidu.com");
    if (addrs.size() < 1) {
        SYLAR_LOG_DEBUG(g_logger) << "resolve error";
        return;
    }
    sylar::IPAddress::ptr addr = std::dynamic_pointer_cast<sylar::IPAddress>(addrs[0]);
    if (!addr) {
        SYLAR_LOG_DEBUG(g_logger) << "dynamic_cast failed";
        return;
    }

    //addr = sylar::IPv4Address::Create("127.0.0.1");
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    addr->setPort(80);
    SYLAR_LOG_DEBUG(g_logger) << addr->toString();

    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect failed " << addr->toString()
                                  << " errno: " << strerror(errno);
        return;
    } else {
        //SYLAR_LOG_DEBUG(g_logger) << "connect success";
    }

    const char buff[] = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\n\r\n";
    //const char buff[] = "GET /2.html HTTP/1.1\r\nHost: 127.0.0.1:90\r\n\r\n";
    int ret = sock->send(buff, strlen(buff));
    if (ret <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "send failed ret: " << ret;
        return;
    } else if (ret == (int)strlen(buff)) {
        SYLAR_LOG_DEBUG(g_logger) << " send request ok " << ret;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "send incompletable ret: " << ret;
    }

    // We should check whether buff send all ok. so we SHOULD SHOULD SHOULD check ret == strlen(buff)

    sylar::Buffer::ptr buffer(new sylar::Buffer);
    sylar::http::HttpResponseParser parser;
    size_t header_length = 0;
    std::ofstream   m_filestream;
    int once_flag = 1;

    while( (ret = sock->recv(buffer->beginWrite(), buffer->writableBytes() - 1)) > 0) {

        buffer->ensuerWritableBytes(buffer->writableBytes() + ret);
        buffer->hasWritten(ret);
        SYLAR_LOG_DEBUG(g_logger) << "ret: " << ret << buffer->peek();
        if (!parser.isFinished()) {
            header_length = parser.execute(buffer->peek(), buffer->readableBytes());
            SYLAR_LOG_DEBUG(g_logger) << "parsed: " << header_length;
        }
        if (parser.isFinished() && once_flag == 1) {
            buffer->retrieve(header_length - 1);
            once_flag = 0;
        }
        if (parser.isFinished() && parser.getContentLength() > 0) {
            SYLAR_LOG_DEBUG(g_logger) << "parser content-length: " << parser.getContentLength()
            << "  buffer->readableBytes(): " << buffer->readableBytes();
            if (parser.getContentLength() == buffer->readableBytes()) {
                break;
            }
        }

        // operate data
    }

    if (parser.isFinished()) {
        //SYLAR_LOG_DEBUG(g_logger)<< parser.getData()->toString();
    }

    if (ret == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "recv: err: " << strerror(errno);
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "recv done ret: " << ret
        << " cl: " << buffer->readableBytes()
        << " parserd cl: " << parser.getData()->getHeader("Content-Length", "nullptr");
        m_filestream.open("1.html", std::ios::trunc);
        if (!m_filestream) {
            SYLAR_LOG_DEBUG(g_logger) << "open file failed";
            return;
        }
        m_filestream << buffer->peek();
        m_filestream.flush();
    }
}

int main() {
    sylar::IOManager iom(1, false, "io");
    for (int i = 0; i < 1; i++) {
        iom.schedule(test_socket);
    }
    iom.stop();
    return 0;
}
