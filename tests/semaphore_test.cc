#include "log.hh"
#include "iomanager.hh"
#include "thread.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void producer(sylar::FiberSemaphore::ptr sem) {
    sleep(5);
    SYLAR_LOG_DEBUG(g_logger) << "producer begin producer...";
    sem->notify();
    return;
}

void consumer(sylar::FiberSemaphore::ptr sem) {
    sem->wait();
    SYLAR_LOG_DEBUG(g_logger) << "consumer wait end, begin consumer...";
    return;
}

int main() {
    sylar::IOManager iom(1, false, "io");
    sylar::FiberSemaphore::ptr sem(new sylar::FiberSemaphore(0));
    iom.schedule(std::bind(&producer, sem));
    iom.schedule(std::bind(&consumer, sem));
    iom.stop();
    return 0;
}
