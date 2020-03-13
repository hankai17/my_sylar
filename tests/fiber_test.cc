#include "log.hh"
#include "thread.hh"
#include <vector>
#include "config.hh"
#include "fiber.hh"
#include "scheduler.hh"
#include <sys/time.h>

// Test fiber should not use scheduler


int count = 0;

// Test fiber should not use scheduler

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
long start_time  = 0;
void run_in_fiber() {
    while(1) {
        sylar::Fiber::YeildToHold();
        count++;
        if (count > 1000000) {
            struct timeval l_now1 = {0};
            gettimeofday(&l_now1, NULL);
            long end_time = ((long)l_now1.tv_sec)*1000+(long)l_now1.tv_usec/1000;
            std::cout << "end: " << end_time << "  elaspse: " << end_time - start_time << std::endl;
            break;
        }
    }
}

void test_fiber() {
    sylar::Fiber::GetThis();
    sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));
    struct timeval l_now = {0};
    gettimeofday(&l_now, NULL);
    start_time = ((long)l_now.tv_sec)*1000+(long)l_now.tv_usec/1000;
    std::cout << "start: " << start_time << std::endl;
    while(1) {
        fiber->swapIn();
        count++;
        if (count > 1000000) {
            struct timeval l_now1 = {0};
            gettimeofday(&l_now1, NULL);
            long end_time = ((long)l_now1.tv_sec)*1000+(long)l_now1.tv_usec/1000;
            std::cout << "end: " << end_time << "  elaspse: " << end_time - start_time << std::endl;
            break;
        }
    }
}

void run_in_fiber1() {
    SYLAR_LOG_DEBUG(g_logger) << "fiber hello world begin";
    sylar::Fiber::YeildToHold();
    SYLAR_LOG_DEBUG(g_logger) << "fiber hello world begin2";
    sylar::Fiber::YeildToHold();
    SYLAR_LOG_DEBUG(g_logger) << "fiber hello world begin4";
    SYLAR_LOG_DEBUG(g_logger) << "fiber hello world end";
}

void test_fiber1() {
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

