#include "fiber.hh"
#include "macro.hh"
#include "log.hh"
#include "config.hh"
#include "scheduler.hh"
#include "util.hh"

#include <atomic>

namespace sylar {
    static thread_local Fiber* t_fiber = nullptr;
    static thread_local Fiber::ptr t_kernel_fiber = nullptr;
    static std::atomic<uint64_t> s_fiber_count {0};
    static std::atomic<uint64_t> s_fiber_id {0};

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    //static sylar::ConfigVar<uint32_t>::ptr g_fiber_stack_size =
    sylar::ConfigVar<uint32_t>::ptr g_fiber_stack_size =
            sylar::Config::Lookup<uint32_t>("fiber.stacksize", 1024 * 1024, "fiber stack size");

    class MallocStackAllocator {
    public:
        static void* Alloc(size_t size) {
            return malloc(size);
        }

        static void Dealloc(void* p, size_t size) {
            return free(p);
        }
    };

    using StackAllocator = MallocStackAllocator;

    void Fiber::SetThis(sylar::Fiber *f) {
        t_fiber = f;
    }

    Fiber::ptr Fiber::GetThis() { // Factory, Must called in every thread
        if (t_fiber) {
            return t_fiber->shared_from_this(); // Not use std::make_shared<Fiber>(*t_fiber);  // Unspoken words: t_fiber already became a sharedptr
        }
        Fiber::ptr main_fiber(new Fiber);
        SetKernelFiber(main_fiber);
        SYLAR_ASSERT(t_fiber == main_fiber.get());
        return t_fiber->shared_from_this();
    }

    void Fiber::SetKernelFiber(Fiber::ptr f) {
        t_kernel_fiber = f;
    }

    Fiber::ptr Fiber::GetKernelFiber() {
        return t_kernel_fiber;
    }

    uint64_t Fiber::GetFiberId() {
        if (t_fiber) {
            return t_fiber->getFiberId();
        }
        return 0;
    }

    Fiber::Fiber() {
        m_id = ++s_fiber_id;
        m_state = EXEC;
        SetThis(this);
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT(false); // TODO sylar_assert2
        }
        ++s_fiber_count;
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber";
    }

    //extern sylar::ConfigVar<uint32_t>::ptr g_fiber_stack_size; // SO ugly!
    Fiber::Fiber(std::function<void()> cb, size_t stacksize)
            : m_id(++s_fiber_id),
              m_cb(cb) {
        ++s_fiber_count;
        m_stacksize = stacksize ? stacksize :  g_fiber_stack_size->getValue();

        m_stack = StackAllocator::Alloc(m_stacksize);
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT(false); // TODO sylar_assert2
        }
        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;
        makecontext(&m_ctx, &Fiber::MainFunc, 0);

        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber id: " << m_id; // Not use s_fiber_id
    }

    void Fiber::swapIn() {
        SetThis(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
        //if (swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx))
        if (swapcontext(&t_kernel_fiber->m_ctx, &m_ctx)) {
            SYLAR_ASSERT(false);
        }
    }

    void Fiber::swapOut() {
        //SetThis(Scheduler::GetMainFiber());
        SetThis(Fiber::GetKernelFiber().get());
        //if (swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx))
        if (swapcontext(&m_ctx, &t_kernel_fiber->m_ctx)) {
                SYLAR_ASSERT(false);
            }
    }


    void Fiber::YeildToReady() {
        Fiber::ptr cur = GetThis();
        cur->m_state = READY;
        cur->swapOut();
    }

    void Fiber::YeildToHold() {
        Fiber::ptr cur = GetThis();
        cur->m_state = HOLD;
        cur->swapOut();
    }

    Fiber::~Fiber() {
        --s_fiber_id;
        if (m_stack) {
            SYLAR_ASSERT(m_state == INIT
                         || m_state == TERM
                         || m_state == EXCEPT)
            StackAllocator::Dealloc(m_stack, m_stacksize);
        } else {
            // TODO
            //SYLAR_ASSERT(!m_cb);
        }
        SYLAR_LOG_DEBUG(SYLAR_LOG_NAME("system")) << "Fiber::~Fiber id: " << m_id;
    }


    void Fiber::reset(std::function<void()> cb) { // So confuse why need this
        SYLAR_ASSERT(m_stack)
        SYLAR_ASSERT(m_state == TERM
                     || m_state == EXCEPT
                     || m_state == INIT);
        m_cb = cb;
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT("getcontext");
        }

        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;

        makecontext(&m_ctx, &Fiber::MainFunc, 0);
        m_state = INIT;
    }

    uint64_t Fiber::TotalFibers() {
        return s_fiber_count;
    }

    void Fiber::MainFunc() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try {
            cur->m_cb();
            cur->m_cb = nullptr;
            cur->m_state = TERM;
        } catch (std::exception& ex) {
            cur->m_state = EXCEPT;
            SYLAR_LOG_ERROR(SYLAR_LOG_NAME("system")) << "Fiber EXCEPT: " << ex.what()
                                                      << " Fiber id: " << cur->getFiberId()
                                                      << std::endl
                                                      << sylar::BacktraceToString();
        } catch (...) {
            cur->m_state = EXCEPT;
            SYLAR_LOG_ERROR(SYLAR_LOG_NAME("system")) << "Fiber EXCEPT: "
                                                      << " Fiber id: " << cur->getFiberId()
                                                      << std::endl
                                                      << sylar::BacktraceToString();
        }
        auto raw_ptr = cur.get(); // Why use raw ptr
        cur.reset();
        raw_ptr->swapOut();
        //SYLAR_ASSERT("never reach there, Fiber id: " + std::to_string(raw_ptr->getFiberId()));
        SYLAR_ASSERT(false);
    }

}