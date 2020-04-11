#include "thread.hh"
#include "log.hh"
#include "util.hh"
#include "macro.hh"
#include "scheduler.hh"
#include "fiber.hh"

namespace sylar {

    static thread_local Thread* t_thread = nullptr;
    static thread_local std::string t_thread_name = "UNKNOW";
    //static sylar::Logger::ptr logger = SYLAR_LOG_NAME("root");
    //static sylar::Logger::ptr logger = SYLAR_LOG_ROOT(); // Why not here

    Semaphore::Semaphore(uint32_t count) {
        if (sem_init(&m_semaphore, 0, count)) {
            throw std::logic_error("sem_init failed");
        }
    }

    Semaphore::~Semaphore() {
        sem_destroy(&m_semaphore);
    }

    void Semaphore::wait() {
        if (sem_wait(&m_semaphore)) {
            throw std::logic_error("sem_wait failed");
        }
    }

    void Semaphore::notify() {
        if (sem_post(&m_semaphore)) {
            throw std::logic_error("sem_wait failed");
        }
    }

    std::string& Thread::GetName() {
        return t_thread_name;
    }

    Thread* Thread::GetThis() { // Why pointer?
        return t_thread;
    }

    void* Thread::run(void* arg) {
        Thread* thread = (Thread*)arg;
        t_thread = thread;
        t_thread_name = thread->m_name;
        thread->m_id = sylar::GetThreadId();
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

        thread->m_semphore.notify(); // Why here
        thread->m_cb(); // Why use swap()

        /*
        std::function<void()> cb;
        cb.swap(thread->m_cb);
        thread->m_semphore.notify(); // Why here
        cb();
         */
        return 0;
    }

    Thread::Thread(std::function<void()> cb, const std::string& name)
    : m_cb(cb),
    m_name(name) {
        if (name.empty()) {
            m_name = "UNKNOW";
        }
        int ret = pthread_create(&m_thread, nullptr, &Thread::run, this);
        if (ret) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "pthread create failed, ret = " << ret
            << " name = " << name;
            throw std::logic_error("pthread create failed");
        }
        m_semphore.wait();
    }

    void Thread::join() {
        if (m_thread) {
            int ret = pthread_join(m_thread, nullptr);
            if (ret) {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "pthread join failed, ret = " << ret
                                                  << " name = " << m_name;
                throw std::logic_error("pthread join failed");
            }
            m_thread = 0;
        }
    }

    Thread::~Thread() {
        if (m_thread) {
            pthread_detach(m_thread);
        }
    }

    void Thread::SetName(const std::string &name) {
        if (t_thread) {
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }

    FiberSemaphore::FiberSemaphore(size_t initalConcurrency)
    : m_concurrency(initalConcurrency) {
    }

    FiberSemaphore::~FiberSemaphore() {
        SYLAR_ASSERT(m_waiters.empty());
    }

    bool FiberSemaphore::tryWait() {
        SYLAR_ASSERT(Scheduler::GetThis());
        {
            MutexType::Lock lock(m_mutex);
            if (m_concurrency > 0u) {
                --m_concurrency;
                return true;
            }
            return false;
        }
    }

    void FiberSemaphore::wait() {
        SYLAR_ASSERT(Scheduler::GetThis());
        {
            MutexType::Lock lock(m_mutex);
            if (m_concurrency > 0u) {
                --m_concurrency;
                return;
            }
            m_waiters.push_back(std::make_pair(Scheduler::GetThis(), Fiber::GetThis()));
        }
        Fiber::YeildToHold();
    }

    void FiberSemaphore::notify() {
        MutexType::Lock lock(m_mutex);
        if (!m_waiters.empty()) {
            auto next = m_waiters.front();
            m_waiters.pop_front();
            next.first->schedule(next.second);
        } else {
            ++m_concurrency;
        }
    }

    static void parallelDoImpl(std::function<void()> defer, int& completed, size_t totalDefers,
            Scheduler* scheduler, Fiber::ptr fiber) {
        try {
            defer();
        } catch (...) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "parallelDoImpl falied";
        }
        //auto idx = sylar::Atomic::addFetch(m_idx, 1);
        if (Atomic::fetchAdd(completed, 1) == (int)totalDefers) {
            scheduler->schedule(fiber);
        }
    }

    void ParallelDo(const std::vector<std::function<void()>>& deferGroups,
                    std::vector<Fiber::ptr>& fibers) {
        SYLAR_ASSERT(fibers.size() >= deferGroups.size());
        int completed = 0;
        Scheduler* scheduler = Scheduler::GetThis();
        Fiber::ptr caller = Fiber::GetThis();
        if (scheduler == nullptr || deferGroups.size() <= 1) {
            for (const auto& i : deferGroups) {
                i();
            }
            return;
        }

        for (size_t i = 0; i < deferGroups.size(); i++) {
#if FIBER_MEM_TYPE == FIBER_MEM_NORMAL
            fibers[i].reset(new Fiber());
#elif FIBER_MEM_TYPE == FIBER_MEM_POOL
            fibers[i].reset( NewFiber(
                            std::bind(&parallelDoImpl, deferGroups[i], std::ref(completed),  // https://blog.csdn.net/zgaoq/article/details/82152713
                                    deferGroups.size(), scheduler, caller)
                    ), FreeFiber);
#endif
            scheduler->schedule(fibers[i]);
        }
        Fiber::YeildToHold();
    }

}
