#include "timer.hh"
#include "log.hh"
#include "iomanager.hh"

#include <sys/epoll.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/eventfd.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
sylar::Timer::ptr g_timer = nullptr;

void test_timer() {
    return;
}

int main() {
    SYLAR_LOG_DEBUG(g_logger) << "Timer test";
    sylar::IOManager io(1, false, "iomanager timer test");
    g_timer = io.addTimer(1000, []() {
        static int i = 0;
        SYLAR_LOG_DEBUG(g_logger) << "Timer test, i: " << i;
        g_timer->refresh();
        if (i++ == 3) {
            g_timer->reset(2000, true); 
            //g_timer->cancel();
        }
    }, true);
    //}, false);
    io.stop();
    return 0;
}
