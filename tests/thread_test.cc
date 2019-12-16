#include "log.hh" // for SYLAR macro
#include "thread.hh"
#include <vector>

void fun1() {
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "thread name: " << sylar::Thread::GetName()
        << " this.getName: "  << sylar::Thread::GetThis()->getName()
        << " id: " << sylar::GetThreadId()
        << " this.id: " << sylar::Thread::GetThis()->getId();
    sleep(5);
}

sylar::RWMutex s_mutex;
int count = 0;

void fun2() {
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "thread name: " << sylar::Thread::GetName()
                                     << " this.getName: "  << sylar::Thread::GetThis()->getName()
                                     << " id: " << sylar::GetThreadId()
                                     << " this.id: " << sylar::Thread::GetThis()->getId();
    for (int i = 0; i < 1000; i++) {
        //s_mutex.wrlock();
        sylar::RWMutex::WriteLock lock(s_mutex);
        count++;
        //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "after count++: " << count;
    }
}

int main() {
    sylar::Logger::ptr logger = SYLAR_LOG_NAME("root");
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "thread test";

    std::vector<sylar::Thread::ptr> vec;
    for (const auto& i : {0, 1, 2, 3, 4}) {
      //sylar::Thread::ptr thr(new sylar::Thread(&fun1, "name_" + std::to_string(i)));
      sylar::Thread::ptr thr(new sylar::Thread(&fun2, "name_" + std::to_string(i)));
      vec.push_back(thr);
    }

    for (const auto& i : {0, 1, 2, 3, 4}) {
        vec[i]->join();
    }

    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "thread test count: " << count;
    return 0;
}
