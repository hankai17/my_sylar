#include "scheduler.hh"
#include "macro.hh"

namespace sylar {
    static thread_local Scheduler* t_scheduler = nullptr;
    static thread_local Fiber* t_scheduler_fiber = nullptr;

    Fiber* Scheduler::GetMainFiber() {
        return t_scheduler_fiber;
    }

    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
    : m_name(name) {
        SYLAR_ASSERT(threads > 0);
    }

    Scheduler::~Scheduler() {

    }

    Scheduler* Scheduler::GetThis() {
        return t_scheduler; // Why not same as fiber.cc
    }

    void Scheduler::start() {

    }

    void Scheduler::stop() {

    }

    void Scheduler::setThis() {
        t_scheduler = this;
    }

    void Scheduler::run() {

    }

    void Scheduler::tickle() {

    }

    bool Scheduler::stopping() {
        return false;
    }

    void Scheduler::idle() {

    }




}