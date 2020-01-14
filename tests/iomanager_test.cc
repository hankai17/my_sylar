#include "iomanager.hh"
#include "log.hh"

#include <sys/epoll.h>
#include <iostream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void true_io_test1() {
    SYLAR_LOG_DEBUG(g_logger) << "test socket";
}
// 1为什么有时候能运行test_socket 有时候却不行 run里遍历的时候可能还没有schedule进去 run里是空 所以先执行idle了
// remove tickle() because of pipe always blocking

void true_io_test2() {
    SYLAR_LOG_DEBUG(g_logger) << "test socket";
}

int main() {
    SYLAR_LOG_DEBUG(g_logger) << "EPOLLIN: " << EPOLLIN << "  EPOLLOUT: " << EPOLLOUT;
    sylar::IOManager io(1, false, "iomanager");
    io.schedule(&true_io_test1);
    io.stop();
    return 0;
}

