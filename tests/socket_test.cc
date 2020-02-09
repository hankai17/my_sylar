#include "socket.hh"
#include "log.hh"
#include "iomanager.hh"
#include "address.hh"
#include <vector>
#include <boost/lexical_cast.hpp>
#include <fstream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
static std::atomic<int> count = {0};
static int req_count = 10;
static uint64_t start_ms = 0;
static uint64_t end_ms = 0;

void test_socket(int index) {
    //SYLAR_LOG_DEBUG(g_logger) << "index begin: " << index;
    /*
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
    // Must IPv4
    */
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1");
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    addr->setPort(90);
    //SYLAR_LOG_DEBUG(g_logger) << addr->toString();

    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "index: " << index << "  connect failed " << addr->toString()
        << " errno: " << strerror(errno);
        count++;
        return;
    } else {
        //SYLAR_LOG_DEBUG(g_logger) << "connect success";
    }

    //const char buff[] = "GET / HTTP/1.1\r\n\r\n";
    //const char buff[] = "GET / HTTP/1.1\r\nHost: www.ifeng.com\r\n\r\n";
    //const char buff[] = "GET http://127.0.0.1:90/2.html HTTP/1.1\r\nHost: 127.0.0.1:90\r\n\r\n";
    const char buff[] = "GET /2.html HTTP/1.1\r\nHost: 127.0.0.1:90\r\n\r\n";
    //int ret = sock->send(buff, sizeof(buff));
    int ret = sock->send(buff, strlen(buff));
    if (ret <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "send failed ret: " << ret;
        count++;
        return;
    } else if (ret == (int)strlen(buff)) {
        //SYLAR_LOG_DEBUG(g_logger) << "index: " << index << " send request ok" << ret;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "send incompletable ret: " << ret;
    }

    // We should check whether buff send all ok. so we SHOULD SHOULD SHOULD check ret == strlen(buff)

    std::string buffs;
    buffs.resize(4096);
    int pos = -1;
    int pos1 = -1;
    int recv_sum = 0;
    int cl = 0;
    int hdr_len = 0;
    int dat_flag = 0;
    std::ofstream   m_filestream;

    while( (ret = sock->recv(&buffs[0], buffs.size())) > 0) {
        //SYLAR_LOG_DEBUG(g_logger) << "while ret: " << ret;
        recv_sum += ret;
        if (cl == 0
            && (pos = buffs.find("\r\nContent-Length: ")) != -1) {
            if ((pos1 = buffs.find("\r\n", pos + strlen("\r\nContent-Length: "))) != -1) {
                std::string len = buffs.substr(pos + strlen("\r\nContent-Length: "), pos1 - (pos + strlen("\r\nContent-Length: ")));
                //SYLAR_LOG_DEBUG(g_logger) << "Content-Length: " << len <<".";
                cl = boost::lexical_cast<int>(len.c_str());
                //SYLAR_LOG_DEBUG(g_logger) << "Content-Length: " << cl <<".";
            }
        }
        if (hdr_len == 0 && cl != 0
            && (pos = buffs.find("\r\n\r\n")) != -1) {
            hdr_len = pos + 4;
            m_filestream.open("1.dat" + std::to_string(index), std::ios::trunc);
            if (!m_filestream) {
                SYLAR_LOG_DEBUG(g_logger) << "open file failed";
                return;
            }
            //buffs = buffs.substr(hdr_len, buffs.size() - hdr_len);
            buffs = buffs.substr(hdr_len, ret - hdr_len);
            dat_flag = 1;
            //SYLAR_LOG_DEBUG(g_logger) << "first read cl: " << buffs.size();
        } else if (hdr_len != 0 && cl != 0) {
            buffs = buffs.substr(0, ret);
            dat_flag = 1;
        }
        if (dat_flag) {
            m_filestream << buffs;
            //SYLAR_LOG_DEBUG(g_logger) << "flush size: " << buffs.size();
            //if ((int)buffs.size() != ret) {
                //SYLAR_LOG_DEBUG(g_logger) << "flush size: " << buffs.size() << "ret: " << ret;
            //}
            m_filestream.flush();
        }

        if (recv_sum == (hdr_len + cl)) {
            SYLAR_LOG_DEBUG(g_logger) << "index: " << index << " read done, recv_sum: " << recv_sum;
            break;
        }
        buffs.clear();
        buffs.resize(4096);
    }

    //SYLAR_LOG_DEBUG(g_logger) << "index: " << index << " read done cl: " << cl;
    if (ret == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "index: " << index << "  err: " << strerror(errno);
    } else {
        //SYLAR_LOG_DEBUG(g_logger) << "index: " << index << " done ret: " << ret;
    }

    //SYLAR_LOG_DEBUG(g_logger) << buffs;
    //SYLAR_LOG_DEBUG(g_logger) << "sock.use_count: " << sock.use_count();
    count++;
    if (count == 1) {
        start_ms = sylar::GetCurrentMs();
        SYLAR_LOG_DEBUG(g_logger) << "req begin " << count << " current_time: " << start_ms;
    }
    if (count == req_count) {
        end_ms = sylar::GetCurrentMs();
        SYLAR_LOG_DEBUG(g_logger) << "all req done " << count << " current_time: " << end_ms
        << " elapse: " << end_ms - start_ms;
    }
    m_filestream.close();
}

int main() {
    sylar::IOManager iom(1, false, "io"); //1 thread 1169ms; 2 io threads 628ms
    for (int i = 0; i < req_count; i++) {
        iom.schedule(std::bind(test_socket, i));
    }
    iom.stop();
    return 0;
}