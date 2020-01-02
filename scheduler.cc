#include "scheduler.hh"
#include "macro.hh"
#include <iostream>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    static thread_local Scheduler* t_scheduler = nullptr;

    // global ONLY has one scheduler and it has many threads and all threads have local d_kernel t_fiber
    // global scheduler means main thread. Is main thread need fiber?

    static thread_local Fiber* t_kernel_fiber = nullptr; // MUST equal to fiber.cc's d_kernel // Why use pointer?

    Scheduler* Scheduler::GetThis() {
        return t_scheduler; // Why not same as fiber.cc
    }

    void Scheduler::setThis() {
        t_scheduler = this;
    }

    Fiber* Scheduler::GetMainFiber() {
        return t_kernel_fiber;
    }

    void Scheduler::SetMainFiber(Fiber *fiber) {
        t_kernel_fiber = fiber;
    }

    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
    : m_name(name) {
        SYLAR_ASSERT(threads > 0);
        sylar::Fiber::GetThis(); // Get main thread's d_kernel_fiber
        --threads; // Why --

        SYLAR_ASSERT(GetThis() == nullptr); // Only has a global scheduler
        setThis();

        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this)));
        sylar::Thread::SetName(m_name); // belongs to class can not use in there

        t_kernel_fiber = m_rootFiber.get(); //主线程的kernel fiber 并不像子线程那种kernel fiber

        //m_threadIds

        m_rootThreadId = GetThreadId();
        m_threadCounts = threads;
    }

    Scheduler::~Scheduler() {
        if (GetThis() == this) {
            t_scheduler = nullptr;
        }
    }

    void Scheduler::start() {
        //MutexType::Lock lock(m_mutex);
        SYLAR_ASSERT(m_threads.empty());
        m_threads.resize(m_threadCounts);
        for (auto& i : m_threads) {
            i.reset(new Thread( std::bind(&Scheduler::run, this), m_name) ); //???
            m_threadIds.push_back(i->getId());
        }

        if (m_rootFiber) {
            //m_rootFiber->call();
            //SYLAR_LOG_DEBUG(g_logger) << "call out " << m_rootFiber->getState();
        }
    }

    void Scheduler::run() {
        SYLAR_LOG_INFO(g_logger) << "scheduler run";
        setThis();
        if (sylar::GetThreadId() != m_rootThreadId) { //child threads && child threads' kernel fiber init
            t_kernel_fiber = Fiber::GetThis().get();
        }

        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
        Fiber::ptr cb_fiber;
        FiberAndThread ft;

        while (true) {
            ft.reset();
            bool tickle_me = false;
            {
                MutexType::Lock lock(m_mutex);
                auto it = m_fibers.begin();
                while (it != m_fibers.end()) {
                    if (it->thread != sylar::GetThreadId()) {
                        ++it;
                        tickle_me = true;
                        continue;
                    }
                    SYLAR_ASSERT(it->fiber || it->cb);
                    /*
                    if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
                        ++it;
                        continue;
                    }
                     */

                    ft = *it;
                    m_fibers.erase(it);
                    break; }
            }

            if (tickle_me) {
                tickle();
            }

            if (ft.fiber /*&& fiber state*/) {
                ft.fiber->swapIn();
                ft.fiber->m_state = Fiber::HOLD; // Because of scheduler not friends of fiber
                ft.reset();
            } else if (ft.cb) {
                cb_fiber.reset(new Fiber(ft.cb));
                cb_fiber->swapIn();
                cb_fiber.reset();
            } else {
                SYLAR_LOG_INFO(g_logger) << "idle fiber";
                if (idle_fiber->getState() == Fiber::TERM) {
                    std::cout<< "idle end, we should break and end the fiber_system. Otherwise it will swapout the mainfun end and core" << std::endl;
                    break;
                }
                idle_fiber->swapIn();
            }
        }
    }

    void Scheduler::stop() {
    }

    void Scheduler::tickle() {
        SYLAR_LOG_INFO(g_logger) << "tickle..";
    }

    bool Scheduler::stopping() {
        return false;
    }

    void Scheduler::idle() {
        SYLAR_LOG_INFO(g_logger) << "idle..";
    }




}
