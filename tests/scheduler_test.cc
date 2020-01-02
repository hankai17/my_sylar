#include "log.hh"
#include "thread.hh"
#include <vector>
#include "config.hh"
#include "fiber.hh"
#include "scheduler.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int main() {
    sylar::Scheduler sc(2, true, "scheduler_test");
    sc.start();
    while(1);
}
