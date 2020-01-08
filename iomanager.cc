#include "iomanager.hh"

namespace sylar {

    IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
    :Scheduler(threads, use_caller, name) {

    }

    int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
        return 0;
    }

    bool IOManager::delEvent(int fd, Event event) {
        return false;
    }

    bool IOManager::cancelEvent(int fd, Event event) {
        return false;
    }

    bool IOManager::cancelAll(int fd) {
        return false;
    }

    IOManager* IOManager::GetThis() {
        return nullptr;
    }

    void IOManager::tickle() {

    }

    bool IOManager::stopping() {
        return false;
    }

    void IOManager::idle() {

    }

    IOManager::~IOManager() {

    }

}

