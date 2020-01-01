#include "log.hh"
#include "thread.hh"
#include <vector>
#include "config.hh"
#include "fiber.hh"
#include "scheduler.hh"

// Test fiber should not use scheduler

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run_in_fiber() {
    SYLAR_LOG_DEBUG(g_logger) << "fiber hello world begin";
    sylar::Fiber::YeildToHold();
    SYLAR_LOG_DEBUG(g_logger) << "fiber hello world begin2";
    sylar::Fiber::YeildToHold();
    SYLAR_LOG_DEBUG(g_logger) << "fiber hello world begin4";
    SYLAR_LOG_DEBUG(g_logger) << "fiber hello world end";
}

void test_fiber() {
    SYLAR_LOG_DEBUG(g_logger) << "main begin";
    {
        sylar::Fiber::GetThis(); // All thread first words that we should get kernel first
        sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber)); // Why not bind // bad weak ptr
        fiber->swapIn();
        SYLAR_LOG_DEBUG(g_logger) << "fiber hello world begin1";
        fiber->swapIn();
        SYLAR_LOG_DEBUG(g_logger) << "fiber hello world begin3";
        fiber->swapIn();
    }
    SYLAR_LOG_DEBUG(g_logger) << "main end";
}

int main() {
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/base_log.yml");
    //sylar::Config::loadFromYaml(root);

    sylar::Thread::setName("main");
    std::vector<sylar::Thread::ptr> thrs;
    int thread_num = 4;
    for (int i = 0; i < thread_num; i++) {
        thrs.push_back(sylar::Thread::ptr(
                new sylar::Thread(&test_fiber, "name_" + std::to_string(i))
        ));
    }

    for (int i = 0; i < thread_num; i++) {
        thrs[i]->join();
    }
}

// That test is simple. If a thread has more than one fiber, That will so difficult to coding
// So scheduler is coming

