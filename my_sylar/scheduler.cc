#include "scheduler.hh"
#include "macro.hh"
#include "hook.hh"
#include <iostream>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static thread_local Scheduler* t_scheduler = nullptr;
    static thread_local Fiber* t_kernel_fiber = nullptr;

    void FiberStatics() {
    }

    Scheduler* Scheduler::GetThis() {
        return t_scheduler; // Why not same as fiber.cc
    }

    void Scheduler::setThis() {
        t_scheduler = this;
    }

    Fiber* Scheduler::GetMainFiber() {
        return t_kernel_fiber; // Only use for child thread
    }

    void Scheduler::SetMainFiber(Fiber *fiber) {
        t_kernel_fiber = fiber;
    }

    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
            : m_name(name) {
        SYLAR_ASSERT(threads > 0);

        if (use_caller) { // use_caller的意思是主线程也跑 是fiber套fiber模型 一般情况下不需要主线程跑
            sylar::Fiber::GetThis();
            --threads; // Why --

            SYLAR_ASSERT(GetThis() == nullptr); // Only has a global scheduler
            setThis();

#if FIBER_MEM_TYPE == FIBER_MEM_NORMAL
            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
#elif FIBER_MEM_TYPE == FIBER_MEM_POOL
            m_rootFiber.reset(NewFiber(std::bind(&Scheduler::run, this), 0, true), FreeFiber);
#endif
            sylar::Thread::SetName(m_name);

            t_kernel_fiber = m_rootFiber.get();

            //m_threadIds
            m_rootThreadId = GetThreadId();
        } else {
          m_rootThreadId = -1;
        }
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
            i.reset(new Thread( std::bind(&Scheduler::run, this), m_name));
            m_threadIds.push_back(i->getId()); // In thread we must wait signal. Otherwise we can not get threadId
        }

        /*
        if (m_rootFiber) {
            m_rootFiber->call();
            //SYLAR_LOG_DEBUG(g_logger) << "call out " << m_rootFiber->getState();
        }
        */
    }
    void Scheduler::switchTo(int thread) {
        SYLAR_ASSERT(Scheduler::GetThis());
        if (Scheduler::GetThis() == this) {
            if (thread == -1 || thread == sylar::GetThreadId()) {
                return;
            }
        }
        schedule(Fiber::GetThis(), thread);
        Fiber::YeildToHold();
    }

    std::ostream& Scheduler::dump(std::ostream& os) {
        os << "[Scheduler name: " << m_name
        << ", size: " << m_threadCounts
        << ", active_count: " << m_activeFiberCount
        << ", idle_count: " << m_activeIdleFiberCount
        << ", fibers in list: " << getFTSize()
        << "]";
        /*
        for (size_t i = 0; i < m_threadCounts; i++) {
            if (i) {
                os << ", ";
            }
            os << m_threadIds[i];
        }
        */
        return os;
    }

    void Scheduler::run() {
        SYLAR_LOG_INFO(g_logger) << "scheduler run";
        set_hook_enable(true);
        setThis();
        if (sylar::GetThreadId() != m_rootThreadId) {
            t_kernel_fiber = Fiber::GetThis().get();
        }

#if FIBER_MEM_TYPE == FIBER_MEM_NORMAL
        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
#elif FIBER_MEM_TYPE == FIBER_MEM_POOL
        Fiber::ptr idle_fiber(NewFiber(std::bind(&Scheduler::idle, this)), FreeFiber);
#endif
        Fiber::ptr cb_fiber;
        FiberAndThread ft;

        while (true) {
            ft.reset();
            bool tickle_me = false;
            {
                MutexType::Lock lock(m_mutex);
                for (auto it = m_fibers.begin(); it != m_fibers.end();) {
                    if (it->thread != -1 && sylar::GetThreadId() != it->thread) {
                        ++it;
                        tickle_me = true; // 所属线程可能在idle 让所属线程跑(竞争)
                        continue;
                    }
                    SYLAR_ASSERT(it->fiber || it->cb);
                    if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
                        ++it;
                        continue;
                    }

                    ft = *it;
                    m_fibers.erase(it++);
                    ++m_activeFiberCount;
                    break;
                }
            }

            if (tickle_me) {
                tickle(); // Why tickle?
            }

            if (ft.fiber /*&& fiber state*/) {
                ft.fiber->swapIn();
                --m_activeFiberCount;
                if (ft.fiber->m_state == Fiber::READY) {
                    schedule(ft.fiber);
                } else if (ft.fiber->m_state != Fiber::TERM && 
                      ft.fiber->m_state != Fiber::EXCEPT) {
                    ft.fiber->m_state = Fiber::HOLD; // Fiber EXCEPT: std::bad_alloc Fiber id: 3
                } else {
                    ;
                }
                ft.reset();
            } else if (ft.cb) {
#if FIBER_MEM_TYPE == FIBER_MEM_NORMAL
                cb_fiber.reset(new Fiber(ft.cb));
#elif FIBER_MEM_TYPE == FIBER_MEM_POOL
                cb_fiber.reset(NewFiber(ft.cb), FreeFiber);
#endif
                cb_fiber->swapIn(); // If simple noblock cb, next line the fiber will destruction
                --m_activeFiberCount;
                if (cb_fiber->getState() == Fiber::READY) {
                    schedule(cb_fiber);
                    cb_fiber.reset();
                } else if (cb_fiber->getState() == Fiber::EXCEPT ||
                    cb_fiber->getState() == Fiber::TERM) {
                    cb_fiber->reset(nullptr);
                } else {
                    cb_fiber->m_state = Fiber::HOLD;
                    cb_fiber.reset();
                    //std::cout << "after cb_fiber.reset(), cb_fiber.use_count: " <<
                     //cb_fiber.use_count() << std::endl;
                }
            } else {
                if (idle_fiber->getState() == Fiber::TERM) {
                    //std::cout<< "idle end, we should break and end the fiber_system. Otherwise it will swapout the mainfun end and core" << std::endl;
                    break;
                }
                ++m_activeIdleFiberCount;
                idle_fiber->swapIn(); // Second swapin goto idle_fiber's ctx that means goto after yeildtohold
                --m_activeIdleFiberCount;
                if (idle_fiber->getState() != Fiber::TERM) {
                    idle_fiber->setState(Fiber::HOLD);
                }
            }
        }
    }

    void Scheduler::stop() {
        if (m_rootFiber && m_threadCounts == 0
            && (m_rootFiber->getState() == Fiber::TERM)) {
            SYLAR_LOG_INFO(g_logger) << this << " stopped";
            stopping();
        }

        if (m_rootThreadId != -1) {
            SYLAR_ASSERT(GetThis() == this);
        } else {
            SYLAR_ASSERT(GetThis() != this);
        }
        m_stopping = true;

        std::vector<Thread::ptr> thrs;
        thrs.swap(m_threads);

        for (const auto& i : thrs) {
            i->join(); // Main thread blocks there
        }
    }

    void Scheduler::tickle() {
    }

    bool Scheduler::stopping() {
        return m_autostop && m_stopping && m_fibers.empty()
          && m_activeFiberCount == 0;
    }

    void Scheduler::idle() {
        while(1) {
            sylar::Fiber::YeildToHold();
        }
    }

    SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
        m_caller = Scheduler::GetThis();
        if (target) {
            target->switchTo();
        }
    }

    SchedulerSwitcher::~SchedulerSwitcher() {
        if (m_caller) {
            m_caller->switchTo();
        }
    }

}

