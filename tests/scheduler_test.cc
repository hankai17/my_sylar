#include "log.hh"
#include "thread.hh"
#include <vector>
#include "config.hh"
#include "fiber.hh"
#include "scheduler.hh"
#include <sys/time.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
int count = 0;

void test_fiber() {
    //SYLAR_LOG_INFO(g_logger) << "test in fiber";
    count++;
}

int main1() {
    sylar::Scheduler sc(2, true, "scheduler_test");
    sc.start();
    while(1);
}

int main2() {
    sylar::Scheduler sc;
    int cbs = 5;
    for (int i = 0; i < cbs; i++) {
        sc.schedule(&test_fiber, sylar::GetThreadId());
    }

    struct timeval l_now = {0};
    gettimeofday(&l_now, NULL);
    long start_time = ((long)l_now.tv_sec)*1000+(long)l_now.tv_usec/1000;
    std::cout << "before start cbs:" << " " << cbs << " "<< " " << start_time << std::endl;

    sc.start();

    l_now = {0};
    gettimeofday(&l_now, NULL);
    long end_time =  ((long)l_now.tv_sec)*1000+(long)l_now.tv_usec/1000;
    std::cout<<"after start cbs:"<< " " << cbs << " count:" << count << " " << " " <<end_time << std::endl;
    std::cout<<"elpase: "<<end_time - start_time << std::endl;
    sc.stop();
    std::cout<<"sc.stop() end"<<std::endl;
    //while(1);
    return 0;
}

void test_fiber1() {
    static int j = 5;
    SYLAR_LOG_INFO(g_logger) << "test_fiber1 j:" << j;
    if (j-- > 0) {
        sylar::Scheduler::GetThis()->schedule(&test_fiber1, sylar::GetThreadId());
    }
}

int main() {
    sylar::Scheduler sc(1, false, "real test");
    sc.start();
    sleep(1);
    sc.schedule(&test_fiber1);
    //while(1);
    sc.stop();
    return 0;
}
