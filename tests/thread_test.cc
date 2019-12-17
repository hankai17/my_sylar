#include "log.hh" // for SYLAR macro
#include "thread.hh"
#include <vector>
#include "config.hh"

void fun1() {
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "thread name: " << sylar::Thread::GetName()
        << " this.getName: "  << sylar::Thread::GetThis()->getName()
        << " id: " << sylar::GetThreadId()
        << " this.id: " << sylar::Thread::GetThis()->getId();
    sleep(5);
}

sylar::RWMutex s_mutex;
int count = 0;
sylar::Logger::ptr file_logger;

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

void fun3() {
    //while (1)
    for (int i = 0; i < 100000; i++) {
        count++;
        SYLAR_LOG_DEBUG(file_logger) << "after count++: " << count;
    }
}

int main1() {
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


int main() {
    file_logger = SYLAR_LOG_NAME("root1");
    //YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/base_log.yml");
    //sylar::Config::loadFromYaml(root);
    file_logger->addAppender(sylar::LogAppender::ptr(new sylar::FileLogAppender("melon.log")));

    SYLAR_LOG_DEBUG(file_logger) << "log thread test";

    std::vector<sylar::Thread::ptr> vec;
    for (const auto& i : {0, 1}) {
        sylar::Thread::ptr thr(new sylar::Thread(&fun3, "name_" + std::to_string(i)));
        vec.push_back(thr);
    }

    for (const auto& i : {0, 1}) {
        vec[i]->join();
    }
    return 0;
}
