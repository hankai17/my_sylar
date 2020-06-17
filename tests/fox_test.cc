#include "my_sylar/log.hh"
#include "my_sylar/db/fox_thread.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test1() {
    SYLAR_LOG_DEBUG(g_logger) << "test1";
}

void test2(int a) {
    SYLAR_LOG_DEBUG(g_logger) << "test2 a: " << a;
}

int main() {
    sylar::FoxThread fthread("fox", NULL);
    fthread.start(); // 起一个线程 event_base_loop

    {
        fthread.dispatch(test1);
        fthread.dispatch(std::bind(test2, 9));

    }

    //fthread.stop();
    while(1);
    return 0;
}

