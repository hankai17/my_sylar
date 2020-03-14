#include "log.hh"
#include "iomanager.hh"
#include "fiber.hh"
#include "util.hh"
#include "scheduler.hh"

#include <iostream>
#include <list>
#include "thread.hh"

int count = 0;
long begin = 0;

void test() {
    sylar::Fiber::ptr curr_fiber = sylar::Fiber::GetThis();
    if (count == 0) {
        begin = sylar::GetCurrentUs();
        //std::cout << "begin: " << begin;
    }
    while (1) {
        count++;
        if (count > 1000000/2) {
            long end = sylar::GetCurrentUs();
            std::cout << "end - begin: " << end - begin << std::endl; // fcontext 90ns; ucontext 250ns
            break;
        }
        sylar::IOManager::GetThis()->schedule((sylar::Fiber::GetThis()));
        //sylar::IOManager::GetThis()->schedule(&curr_fiber);
        sylar::Fiber::YeildToHold();
    }
}

void test_list() {
    std::list<int> l;
    long begin = sylar::GetCurrentUs();
    for (int i = 0; i < 1000000; i++) {
        l.push_back(i);
    }
    long end = sylar::GetCurrentUs();
    std::cout << "list insert 100W consume: " << end - begin << std::endl;

    for (int i = 0; i < 1000000; i++) {
        l.pop_front();
    }
    begin = sylar::GetCurrentUs();
    std::cout << "list pop_front 100W consume: " << begin - end << std::endl;

    std::list<int> l1;
    sylar::Mutex m;
    for (int i = 0; i < 1000000; i++) {
        sylar::Mutex::Lock lock(m);
        l.push_back(i);
        l.pop_front();
    }
    end = sylar::GetCurrentUs();
    std::cout << "list push_back & pop_front 100W consume: " << end - begin << std::endl;
}

int main() {
    test_list();
    sylar::IOManager* iom(new sylar::IOManager(1, false, "io"));
    iom->schedule(test);
    iom->stop();
    return 0;
}
