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
    sylar::Address::Lookup(addrs, "www.ifeng.com");
    //sylar::Address::Lookup(addrs, "www.baidu.com");
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

    //const char buff[] = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\n\r\n";
    const char buff[] = "GET / HTTP/1.1\r\nHost: www.ifeng.com\r\n\r\n";
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

//#define USER_MKSPACE_PRODUCER_CONSUMER 0

#ifdef USER_MKSPACE_PRODUCER_CONSUMER
    while( (ret = sock->recv(buffer->beginWrite(), buffer->writableBytes() - 1)) > 0) {

        buffer->ensuerWritableBytes(buffer->writableBytes() + ret);
        buffer->hasWritten(ret);
        //SYLAR_LOG_DEBUG(g_logger) << "ret: " << ret << buffer->peek();
        if (!parser.isFinished()) {
            header_length = parser.execute(buffer->peek(), buffer->readableBytes());
            SYLAR_LOG_DEBUG(g_logger) << "parsed: " << header_length;
        }
        if (parser.isFinished() && once_flag == 1) {
            SYLAR_LOG_DEBUG(g_logger) << "parsed finished, Content-Length: "
            << parser.getContentLength()
            << "   " << parser.getData()->getHeader("content-length", "null");
            SYLAR_LOG_DEBUG(g_logger) << parser.getData()->toString();
            std::stringstream ss; parser.getData()->dump(ss); SYLAR_LOG_DEBUG(g_logger) << ss.str();
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
#else
    int errnos;
    while((ret = buffer->readFd(sock->getSocket(), &errnos)) > 0) {
        if (!parser.isFinished()) {
            header_length = parser.execute(buffer->peek(), buffer->readableBytes());
            SYLAR_LOG_DEBUG(g_logger) << "parsed: " << header_length;
        }
        if (parser.isFinished() && once_flag == 1) {
            SYLAR_LOG_DEBUG(g_logger) << "parsed finished, Content-Length: "
                                      << parser.getContentLength()
                                      << "   " << parser.getData()->getHeader("content-length", "null");
            SYLAR_LOG_DEBUG(g_logger) << parser.getData()->toString();
            std::stringstream ss; parser.getData()->dump(ss); SYLAR_LOG_DEBUG(g_logger) << ss.str();
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

#endif

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

void test_throughput(int index) {
     uint64_t btime = sylar::GetCurrentMs();
    auto addr = sylar::IPv4Address::Create("192.168.1.10");
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    addr->setPort(80);

    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect failed " << addr->toString()
                                  << " errno: " << strerror(errno);
        return;
    } else {
        //SYLAR_LOG_DEBUG(g_logger) << "connect success";
    }

    const char buff[] = "GET /1.rar HTTP/1.1\r\nHost: 192.168.1.10\r\n\r\n";
    int ret = sock->send(buff, strlen(buff));
    if (ret <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "send failed ret: " << ret;
        return;
    } else if (ret == (int)strlen(buff)) {
        SYLAR_LOG_DEBUG(g_logger) << " send request ok " << ret;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "send incompletable ret: " << ret;
    }
    //sock->setRecvTimeout(5000); // TODO

    // We should check whether buff send all ok. so we SHOULD SHOULD SHOULD check ret == strlen(buff)

    sylar::Buffer::ptr buffer(new sylar::Buffer);
    sylar::http::HttpResponseParser parser;
    size_t header_length = 0;
    std::ofstream   m_filestream;
    int once_flag = 1;

//#define USER_MKSPACE_PRODUCER_CONSUMER 0

#ifdef USER_MKSPACE_PRODUCER_CONSUMER
    while( (ret = sock->recv(buffer->beginWrite(), buffer->writableBytes() - 1)) > 0) {

        buffer->ensuerWritableBytes(buffer->writableBytes() + ret);
        buffer->hasWritten(ret);
        //SYLAR_LOG_DEBUG(g_logger) << "ret: " << ret << buffer->peek();
        if (!parser.isFinished()) {
            header_length = parser.execute(buffer->peek(), buffer->readableBytes());
            SYLAR_LOG_DEBUG(g_logger) << "parsed: " << header_length;
        }
        if (parser.isFinished() && once_flag == 1) {
            SYLAR_LOG_DEBUG(g_logger) << "parsed finished, Content-Length: "
            << parser.getContentLength()
            << "   " << parser.getData()->getHeader("content-length", "null");
            SYLAR_LOG_DEBUG(g_logger) << parser.getData()->toString();
            std::stringstream ss; parser.getData()->dump(ss); SYLAR_LOG_DEBUG(g_logger) << ss.str();
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
#else
    int errnos;
    uint64_t sum_flow = 0;
    while((ret = buffer->orireadFd(sock->getSocket(), &errnos)) > 0) {
        //buffer->retrieve(ret); // Best performance: 330MB/s
        if (!parser.isFinished()) { // 如果buffer一直增 就会230MB/s 如果buffer消费的快就达到瓶颈330MB/s
            std::string header(buffer->peek(), buffer->readableBytes());
            //header_length = parser.execute(&header[0], header.size()); // so dangerous
            header_length = parser.execute(const_cast<char*>(header.c_str()), header.size());
        }
        if (once_flag == 1 && parser.isFinished()) {
            //SYLAR_LOG_DEBUG(g_logger) << "parsed finished, Content-Length: "
                                      //<< parser.getContentLength();
            sum_flow += (ret - header_length + 1);
            once_flag = 2;
        }
        buffer->retrieve(ret);
        if (parser.isFinished() && parser.getContentLength() > 0) {
            if (once_flag == 2) {
                once_flag = 3;
            } else {
                sum_flow += ret;
            }
            if (parser.getContentLength() == sum_flow) {
                break;
            }
        }
    }

#endif

    if (ret == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "recv: err: " << strerror(errno);
    } else {
        uint64_t etime = sylar::GetCurrentMs();
        if (ret == 0) {
            SYLAR_LOG_DEBUG(g_logger) << "index: " << index
            << " ret: 0"
            << " errno: " << errno
            << " " << strerror(errno);
        }
        if (parser.isFinished()) {
            if (parser.getData()->getStatus() == sylar::http::HttpStatus::OK) {
                SYLAR_LOG_DEBUG(g_logger) << "index: " << index << "  ret: " << ret
                << " 200OK time elaspe(ms): " << etime - btime
                                            << " sum: " << sum_flow
                                            << " conent-length: " << parser.getContentLength()
                                          << "  rate(MB/s): " << (parser.getContentLength() / (etime - btime)) * 1000 / 1024 / 1024;
            } else {
                SYLAR_LOG_DEBUG(g_logger) << "index: " << index << "  "
                        << parser.getData()->getReason() << " time elaspe(ms): " << etime - btime
                                          << "  rate(MB/s): " << (parser.getContentLength() / (etime - btime)) * 1000 / 1024 / 1024;
            }
        } else {
            SYLAR_LOG_DEBUG(g_logger) << "index: " << index << " "
            << "parsed failed";
        }
        /*
        m_filestream.open("1.html", std::ios::trunc);
        if (!m_filestream) {
            SYLAR_LOG_DEBUG(g_logger) << "open file failed";
            return;
        }
        m_filestream << buffer->peek();
        m_filestream.flush();
         */
    }
}


int main() {
    sylar::IOManager iom(1, false, "io");
    for (int i = 0; i < 20; i++) {
        //iom.schedule(test_socket);
        iom.schedule(std::bind(test_throughput, i));
    }
    iom.stop();
    return 0;
}
