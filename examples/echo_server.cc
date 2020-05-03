#include "my_sylar/log.hh"
#include "my_sylar/tcp_server.hh"
#include "my_sylar/bytearray.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/buffer.hh"

#include <memory>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class EchoServer : public sylar::TcpServer {
public:
    typedef std::shared_ptr<EchoServer> ptr;
    void handleClient(sylar::Socket::ptr client) {
        SYLAR_LOG_DEBUG(g_logger) << client->toString();
        sylar::Buffer::ptr buf(new sylar::Buffer);
        int err;
        while (true) {
            int ret = buf->orireadFd(client->getSocket(), &err);
            if (ret == 0) {
                SYLAR_LOG_DEBUG(g_logger) << "peer closed";
                break;
            } else if (ret < 0) {
                SYLAR_LOG_DEBUG(g_logger) << "recv errno: " << errno
                << " strerrno: " << strerror(errno);
                break;
            }

            //SYLAR_LOG_DEBUG(g_logger) << buf->peek();
            ret = buf->writeFd(client->getSocket(), ret, &err);
            if (ret == 0) {
                SYLAR_LOG_DEBUG(g_logger) << "peer closed";
                break;
            } else if (ret < 0) {
                SYLAR_LOG_DEBUG(g_logger) << "recv errno: " << errno
                << " strerrno: " << strerror(errno);
                break;
            }
        }
    }
};

void run() {
    SYLAR_LOG_DEBUG(g_logger) << "run";
    EchoServer::ptr es(new EchoServer);
    auto addr = sylar::IPAddress::Create("127.0.0.1", 9527);
    while (!es->bind(addr)) {
        sleep(2);
    }
    es->start();
}

int main() {
    sylar::IOManager iom(1, false, "EchoServer");
    iom.schedule(run);
    iom.stop();
    return 0;
}

// g++ echo_test.cc -I/usr/local/include -lmy_sylar -std=c++11 -lssl -lyaml-cpp
// CentOS Linux release 7.2.1511 (Core)
// Linux 3.10.0-327.el7.x86_64 #1 SMP Thu Nov 19 22:10:57 UTC 2015 x86_64 x86_64 x86_64 GNU/Linux

/*
2020-04-28 09:31:33     4600    4       [DEBUG] [system]        my_sylar/iomanager.cc:343       epoll_ctl ret: 1
2020-04-28 09:31:33     4599    6       [DEBUG] [system]        my_sylar/iomanager.cc:146       epoll_ctl(3, 1, 6, 2147483649):
2020-04-28 09:31:33     4600    8       [DEBUG] [root]  /root/CLionProjects/my_sylar/examples/echo_server.cc:15 [Socket socket: 7 is_connected: 1 family: 2 type: 1 protocol: 0 local_addr: 127.0.0.1:9527 remote_addr: 127.0.0.1:52818]
2020-04-28 09:31:33     4600    8       [DEBUG] [system]        my_sylar/iomanager.cc:146       epoll_ctl(3, 1, 7, 2147483649):
2020-04-28 09:31:33     4599    2       [DEBUG] [system]        my_sylar/iomanager.cc:343       epoll_ctl ret: 1
2020-04-28 09:31:34     4599    2       [DEBUG] [system]        my_sylar/iomanager.cc:343       epoll_ctl ret: 1
2020-04-28 09:31:34     4599    2       [DEBUG] [system]        my_sylar/iomanager.cc:364       ori event.events: 1 fd: 7
2020-04-28 09:31:34     4599    2       [DEBUG] [system]        my_sylar/iomanager.cc:394       epoll_ctl(3, 2, 7, 2147483648):0
2020-04-28 09:31:34     4600    4       [DEBUG] [system]        my_sylar/iomanager.cc:343       epoll_ctl ret: 1
2020-04-28 09:31:34     4599    2       [DEBUG] [system]        my_sylar/iomanager.cc:53        trigger fiber fd: 7 m_id: 8 m_state: 1 event: 1
2020-04-28 09:31:34     4599    8       [DEBUG] [root]  /root/CLionProjects/my_sylar/examples/echo_server.cc:30 GET /2.html HTTP/1.1
Host: 127.0.0.1


2020-04-28 09:31:34     4599    8       [DEBUG] [root]  /root/CLionProjects/my_sylar/examples/echo_server.cc:22 peer closed
2020-04-28 09:31:34     4599    2       [DEBUG] [system]        my_sylar/iomanager.cc:343       epoll_ctl ret: 1
2020-04-28 09:31:34     4600    4       [DEBUG] [system]        my_sylar/iomanager.cc:364       ori event.events: 1 fd: 7
2020-04-28 09:31:34     4600    4       [DEBUG] [system]        my_sylar/iomanager.cc:378       fd_ctx->events: 0 real_events: 1 not insterest, continue
2020-04-28 09:31:37     4599    2       [DEBUG] [system]        my_sylar/iomanager.cc:343       epoll_ctl ret: 0


如果r事件 rfin事件同时触发两个线程的epoll
假设第一个r事件先拿到锁执行 trigger后取消上层关心的r 扔到队列里    (假设扔到队列里立即执行 read数据 readfin close)
第二个拿到锁 发现上层已经不关心此事件(上层可能已经关闭了) continue  ------------那么会漏掉此事  但是et模式 上层才是主导 不会漏掉?

漏事件 可怕吗？  可以允许漏事件吗？
*/

/*
我echo收到数据是r事件没毛病 为什么收到fin还是r事件

*/
