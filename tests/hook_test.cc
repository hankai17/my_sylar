#include "log.hh"
#include "iomanager.hh"
#include "hook.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_sleep() {
    sylar::IOManager* io = sylar::IOManager::GetThis();
    SYLAR_LOG_DEBUG(g_logger) << "start test_sleep";
    io->schedule([](){
        sleep(2);
        SYLAR_LOG_DEBUG(g_logger) << "sleep 2";
    });

    io->schedule([](){
        sleep(3);
        SYLAR_LOG_DEBUG(g_logger) << "sleep 3";
    });
    SYLAR_LOG_DEBUG(g_logger) << "end test_sleep";
}


int main() {
    sylar::IOManager iom(1, false);
    iom.schedule(test_sleep);
    iom.stop();
    return 0;
}
