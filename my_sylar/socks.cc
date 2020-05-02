#include "my_sylar/socks.hh"
#include "my_sylar/log.hh"
#include "my_sylar/util.hh"
#include "my_sylar/macro.hh"
#include "my_sylar/endian.hh"

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    sylar::Stream::ptr tunnel(sylar::Stream::ptr cstream, AresChannel::ptr channel, const std::string& targetIP,
          uint16_t targetPort) {
        std::string buffer;
        buffer.resize(257);

        // read client req
        if (cstream->readFixSize(&buffer[0], 2) <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "read client failed";
            return nullptr;
        }
        // we support 0(noauth) 1(userpas) 80(ssl)
        if (buffer[0] != 5) {
            SYLAR_LOG_ERROR(g_logger) << "we only support version5";
            return nullptr;
        }
        int client_auth_num = buffer[1];
        if (client_auth_num <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "client not support any auth";
            return nullptr;
        }
        if (cstream->readFixSize(&buffer[0], client_auth_num) <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "read client failed";
            return nullptr;
        }

        // we use the first auth default
        if (buffer[0] != 0 && buffer[0] != 2 && buffer[0] != (int8_t)0x80) {
            SYLAR_LOG_ERROR(g_logger) << "ss5 not support auth: " << buffer[0];
            buffer[0] = 0xFF;
            cstream->writeFixSize(&buffer[0], 1);
            return nullptr;
        }
        int default_auth = buffer[0];

        // resp no auth
        buffer[0] = 5;
        buffer[1] = default_auth;
        if (cstream->writeFixSize(&buffer[0], 2) <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "write client failed";
            return nullptr;
        }

        if (default_auth == 0) {
        } else if (default_auth == 1) {
            // compare usepass with para
            if (cstream->readFixSize(&buffer[0], 2) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read client failed";
                return nullptr;
            }
            if (buffer[0] != default_auth) {
                SYLAR_LOG_ERROR(g_logger) << "read client failed";
                return nullptr;
            }
            // get user
            int user_len = buffer[1];
            if (user_len > 255 || cstream->readFixSize(&buffer[0], user_len) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read client failed";
                return nullptr;
            }
            std::string client_user(&buffer[0], user_len);

            // get pas
            if (cstream->readFixSize(&buffer[0], 1) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read client failed";
                return nullptr;
            }
            int pas_len = buffer[0];
            if (cstream->readFixSize(&buffer[0], pas_len) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read client failed";
                return nullptr;
            }
            std::string client_pas(&buffer[0], pas_len);
            // compare with para
        } else {
            // use ssl
        }

        if (targetIP != "") { // p1
            // new ss (return it)
            Address::ptr addr = IPAddress::Create(targetIP.c_str(), targetPort);
            Socket::ptr sock = 0 ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
            if (!sock->connect(addr)) {
                SYLAR_LOG_ERROR(g_logger) << "connect to proxy failed"; 
                return nullptr; 
            }
            sock->setRecvTimeout(1000 * 5);
            Stream::ptr stream(new SocketStream(sock));
            //SYLAR_LOG_DEBUG(g_logger) << "new stream, cstream->fd: "
            //<< std::dynamic_pointer_cast<SocketStream>(cstream)->getSocket()->getSocket()
            //<< " sstream->fd: " << sock->getSocket();

            // Whether has this logic?

            // conn to p2 only support noauth
            buffer[0] = 5;
            buffer[1] = 1;
            buffer[2] = 0;
            if (stream->writeFixSize(&buffer[0], 3) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "send to proxy failed";
                return nullptr;
            }
            // recv resp
            if (stream->readFixSize(&buffer[0], 2) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read proxy failed";
                return nullptr;
            }
            SYLAR_ASSERT(buffer[0] == 5 && buffer[1] == 0);
            return stream;
        }

        // read p1's request

        if (cstream->readFixSize(&buffer[0], 5) <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "read p1 failed";
            return nullptr;
        }
        int addr_len = buffer[4];
        if (buffer[3] == 1) { // IPv4
            if (cstream->readFixSize(&buffer[0], addr_len) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read client addr failed";
                return nullptr;
            }
            if (cstream->readFixSize(&buffer[0], 2) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read client port failed";
                return nullptr;
            }
            uint16_t port = sylar::byteswapOnLittleEndian((*(uint16_t*)&buffer[0]));
            Address::ptr addr = IPAddress::Create(std::string(buffer, addr_len).c_str(), port);
            Socket::ptr sock = 0 ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
            if (!sock->connect(addr)) {
                SYLAR_LOG_ERROR(g_logger) << "connect to proxy failed"; 
                return nullptr; 
            }
            sock->setRecvTimeout(1000 * 5);
            Stream::ptr stream(new SocketStream(sock));
            SYLAR_LOG_DEBUG(g_logger) << "new stream, cstream->fd: "
            << std::dynamic_pointer_cast<SocketStream>(cstream)->getSocket()->getSocket()
            << " sstream->fd: " << sock->getSocket();
            buffer[0] = 5;
            buffer[1] = 0;
            buffer[2] = 0;
            buffer[3] = 1;
            for (int i = 0; i < 6; i++) {
                buffer[4 + i] = 0;
            }
            if (cstream->writeFixSize(&buffer[0], 10) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "stream write to client";
                return nullptr;
            }
            return stream;
        } else if (buffer[3] == 3) { // Domain
            if (cstream->readFixSize(&buffer[0], addr_len) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read client addr failed";
                return nullptr;
            }
            std::string domain(&buffer[0], addr_len);
            std::vector<Address::ptr> result;
            while (channel == nullptr) {
                sleep(2);
            }

            if (cstream->readFixSize(&buffer[0], 2) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "read client port failed";
                return nullptr;
            }
            uint16_t port = sylar::byteswapOnLittleEndian((*(uint16_t*)&buffer[0]));

            sylar::IPAddress::ptr addr = nullptr;
            addrinfo hints, *results;
            memset(&hints, 0, sizeof(addrinfo));
            hints.ai_flags = AI_NUMERICHOST; // 必须是一个数字地址字符串
            hints.ai_family = AF_UNSPEC;
            int err = getaddrinfo(domain.c_str(), NULL, &hints, &results); // 都是阻塞的 默认解析IPV6和IPV4  如果设只解析IPV4速度则很快
            if (err) {
                auto ips = channel->aresGethostbyname(domain.c_str());
                if (ips.size() == 0) {
                    SYLAR_LOG_DEBUG(g_logger) << "dns resolve : " << domain << " failed";
                    return nullptr;
                }
                addr = std::dynamic_pointer_cast<sylar::IPAddress>(
                        sylar::Address::Create(ips[0].getAddr(), ips[0].getAddrLen())
                );
                //SYLAR_LOG_ERROR(g_logger) << "dns resolve done " << domain << " : " << addr->toString();
            } else {
                addr = IPAddress::Create(domain.c_str(), port);
            }


            addr->setPort(port);
            Socket::ptr sock = 0 ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
            if (!sock->connect(addr)) {
                SYLAR_LOG_ERROR(g_logger) << "connect to os: " << domain << " failed"
                << " errno: " << errno << " strerrno: " << strerror(errno);
                return nullptr; 
            }
            sock->setRecvTimeout(1000 * 5);
            Stream::ptr stream(new SocketStream(sock));

            //SYLAR_LOG_ERROR(g_logger) << "new stream, cstream->fd: "
            //<< std::dynamic_pointer_cast<SocketStream>(cstream)->getSocket()->getSocket()
            //<< " sstream->fd: " << sock->getSocket();

            buffer[0] = 5;
            buffer[1] = 0;
            buffer[2] = 0;
            buffer[3] = 1;
            for (int i = 0; i < 6; i++) {
                buffer[4 + i] = 0;
            }
            if (cstream->writeFixSize(&buffer[0], 10) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "stream write to client";
                return nullptr;
            }
            return stream;
        } else if (buffer[4] == 4) { // IPv6
            // TODO
        } else {
            SYLAR_LOG_ERROR(g_logger) << "unknow type: " << buffer[4];
            return nullptr;
        }
         
        return nullptr;
    }
}
