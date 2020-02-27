#include "daemon.hh"
#include "log.hh"
#include "iomanager.hh"
#include "timer.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
sylar::Timer::ptr timer;

int test(int argc, char** argv) {
    SYLAR_LOG_DEBUG(g_logger) << sylar::ProcessInfo::getProcessInfo()->toString();
    sylar::IOManager iom(1, false, "io");
    timer = iom.addTimer(1000, []() {
        SYLAR_LOG_DEBUG(g_logger) << "onTimer";
        static int count = 0;
        if (++count > 10) {
            exit(1);
        }
    }, true);
    iom.stop();
    return 0;
}

int main(int argc, char** argv) {
    return sylar::start_daemon(argc, argv, test, true);
}