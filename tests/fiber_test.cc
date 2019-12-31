#include "log.hh"
#include "thread.hh"
#include <vector>
#include "config.hh"
#include "fiber.hh"
#include "scheduler.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run_in_fiber() {
    //sylar::Fiber::YeildToHold();
    //sylar::Fiber::YeildToHold();
    sylar::Fiber::ptr f = sylar::Fiber::GetThis();
    f->setState(sylar::Fiber::TERM);
}

void test_fiber() {
    SYLAR_LOG_DEBUG(g_logger) << "main begin";
    {
        sylar::Fiber::GetThis();
        sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));
        sylar::Scheduler::SetMainFiber(fiber.get());
        fiber->swapIn();
        //fiber->swapIn();
        //fiber->swapIn();
    }
}

int main() {
    sylar::Thread::setName("main");
    std::vector<sylar::Thread::ptr> thrs;
    int thread_num = 1;
    for (int i = 0; i < thread_num; i++) {
        thrs.push_back(sylar::Thread::ptr(
                new sylar::Thread(&test_fiber, "name_" + std::to_string(i))
        ));
    }

    for (int i = 0; i < thread_num; i++) {
        thrs[i]->join();
    }
}

