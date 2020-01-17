#include "scheduler.hh"
#include "macro.hh"
#include <iostream>
#include <sys/time.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static thread_local Scheduler* t_scheduler = nullptr;
    static thread_local Fiber* t_kernel_fiber = nullptr;

    static int idle_count = 0;
    long start_time = 0; 
    long end_time = 0; 

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

            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, use_caller));
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
            m_threadIds.push_back(i->getId());
        }

        /*
        if (m_rootFiber) {
            m_rootFiber->call();
            //SYLAR_LOG_DEBUG(g_logger) << "call out " << m_rootFiber->getState();
        }
        */
    }

    void Scheduler::run() {
        SYLAR_LOG_INFO(g_logger) << "scheduler run";
        setThis();
        if (sylar::GetThreadId() != m_rootThreadId) {
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
                for (auto it = m_fibers.begin(); it != m_fibers.end();) {
                    if (it->thread != -1 && sylar::GetThreadId() != it->thread) {
                        ++it;
                        tickle_me = true;
                        continue;
                    }
                    SYLAR_ASSERT(it->fiber || it->cb);

                    ft = *it;
                    m_fibers.erase(it);
                    break;
                }
            }

            if (tickle_me) {
                tickle();
            }

            if (ft.fiber /*&& fiber state*/) {
                std::cout<<"0 ft.fiber.use_count: "<<ft.fiber.use_count()<<std::endl;
                ft.fiber->swapIn();
                if (ft.fiber->m_state == Fiber::TERM) {
                    ;
                } else {
                    ft.fiber->m_state = Fiber::HOLD;
                }
                std::cout<<"3 ft.fiber.use_count: "<<ft.fiber.use_count()<<std::endl;
                ft.reset();
            } else if (ft.cb) {
                cb_fiber.reset(new Fiber(ft.cb));
                std::cout<<"before ft.cb swapin, cb_fiber.use_count: "<<cb_fiber.use_count()<<std::endl;
                cb_fiber->swapIn(); // If simple noblock cb, next line the fiber will destruction
                std::cout<<"after ft.cb swapin, cb_fiber.use_count: "<<cb_fiber.use_count()<<std::endl;
                cb_fiber.reset();
            } else {
                SYLAR_LOG_INFO(g_logger) << "idle fiber";
                if (idle_fiber->getState() == Fiber::TERM) {
                    std::cout<< "idle end, we should break and end the fiber_system. Otherwise it will swapout the mainfun end and core" << std::endl;
                    break;
                }
                idle_fiber->swapIn(); // Second swapin goto idle_fiber's ctx that means goto after yeildtohold
                if (idle_count == 0) {
                    struct timeval l_now = {0};
                    gettimeofday(&l_now, NULL);
                    start_time = ((long)l_now.tv_sec)*1000+(long)l_now.tv_usec/1000;
                    std::cout<< "idle_count == 0 start: " << start_time << std::endl;
                }
                idle_count++; 
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
            //std::cout<<"m_rootThreadId: "<<m_rootThreadId<<" GetThis:"<< GetThis() << " this:"<<this<<std::endl;
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
        SYLAR_LOG_INFO(g_logger) << "tickle..";
    }

    bool Scheduler::stopping() {
        return m_stopping && m_fibers.empty();
    }

    void Scheduler::idle() {
        SYLAR_LOG_INFO(g_logger) << "idle..";
        while(1) {
            sylar::Fiber::YeildToHold();
            idle_count++;
            if (idle_count == 1000000) {
                    struct timeval l_now = {0};
                    gettimeofday(&l_now, NULL);
                    end_time = ((long)l_now.tv_sec)*1000+(long)l_now.tv_usec/1000;
                    std::cout<< "idle_count == 0 end: " << end_time << std::endl;
                    std::cout<<idle_count << " elapse " << end_time - start_time << std::endl;
              break;
            }
        }
        //std::cout<<"in idle() after yeildtohold" << std::endl;
    }

}

