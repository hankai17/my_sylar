#include "iomanager.hh"
#include "log.hh"

#include <sys/epoll.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
int sock;

void true_io_test1() {
    SYLAR_LOG_DEBUG(g_logger) << "test socket1";
}
// 1为什么有时候能运行test_socket 有时候却不行 run里遍历的时候可能还没有schedule进去 run里是空 所以先执行idle了
// remove tickle() because of pipe always blocking

void true_io_test2() {
    SYLAR_LOG_DEBUG(g_logger) << "test socket2";
}

void true_io_test3() {
    SYLAR_LOG_DEBUG(g_logger) << "test socket3";
    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "111.206.176.91", &addr.sin_addr.s_addr);

    if (connect(sock, (const sockaddr*)&addr, sizeof(addr)) == 0) {
    } else if (errno == EINPROGRESS) {
        SYLAR_LOG_DEBUG(g_logger) << " connect errno: " << errno << "  " << strerror(errno);
        sylar::IOManager::GetThis()->addEvent(sock, sylar::IOManager::READ, [](){
            SYLAR_LOG_DEBUG(g_logger) << "read callback";
        });
        sylar::IOManager::GetThis()->addEvent(sock, sylar::IOManager::WRITE, [](){
            SYLAR_LOG_DEBUG(g_logger) << "write callback";
            //sylar::IOManager::GetThis()->cancelEvent(sock, sylar::IOManager::READ);
            //close(sock);
        });
    } else {
        SYLAR_LOG_DEBUG(g_logger) << " connect errno: " << errno << " " << strerror(errno);
        close(sock);
    }
}

int main() {
    SYLAR_LOG_DEBUG(g_logger) << "EPOLLIN: " << EPOLLIN << "  EPOLLOUT: " << EPOLLOUT;
    sylar::IOManager io(1, false, "iomanager");
    io.schedule(&true_io_test3);
    io.stop();
    return 0;
}

