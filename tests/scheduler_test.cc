#include "log.hh"
#include "thread.hh"
#include <vector>
#include "config.hh"
#include "fiber.hh"
#include "scheduler.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_fiber() {
    SYLAR_LOG_INFO(g_logger) << "test in fiber";
}

int main1() {
    sylar::Scheduler sc(2, true, "scheduler_test");
    sc.start();
    while(1);
}

int main() {
    sylar::Scheduler sc;
    sc.schedule(&test_fiber, sylar::GetThreadId());
    sc.start();
    std::cout<<"sc.start() end"<<std::endl;
    sc.stop();
    std::cout<<"sc.stop() end"<<std::endl;
}
