#include "iomanager.hh"
#include "log.hh"

#include <sys/epoll.h>
#include <iostream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void true_io_test1() {
    SYLAR_LOG_DEBUG(g_logger) << "test socket";
}
// 1为什么有时候能运行test_socket 有时候却不行

int main() {
    SYLAR_LOG_DEBUG(g_logger) << "EPOLLIN: " << EPOLLIN << "  EPOLLOUT: " << EPOLLOUT;
    sylar::IOManager io(1, false, "iomanager");
    io.schedule(&true_io_test1);
    std::cout<<"1++++++++++++++++"<<std::endl;
    io.stop();
    //std::cout<<"2++++++++++++++++"<<std::endl;
    return 0;
}

