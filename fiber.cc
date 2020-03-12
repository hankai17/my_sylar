#include "fiber.hh"
#include "macro.hh"
#include "log.hh"
#include "config.hh"
#include "scheduler.hh"
#include "util.hh"

#include <atomic>

namespace sylar {
    static thread_local Fiber* t_fiber = nullptr;
    static thread_local Fiber::ptr t_main_thread_fiber = nullptr; // Only use for main thread
    static std::atomic<uint64_t> s_fiber_count {0};
    static std::atomic<uint64_t> s_fiber_id {0};

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
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
        t_main_thread_fiber = main_fiber;
        SYLAR_ASSERT(t_fiber == main_fiber.get());
        return t_fiber->shared_from_this();
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
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT(false); // TODO sylar_assert2
        }
#endif
        ++s_fiber_count;
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber";
    }

    Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller)
            : m_id(++s_fiber_id),
              m_cb(cb) {
        ++s_fiber_count;
        m_stacksize = stacksize ? stacksize :  g_fiber_stack_size->getValue();

        m_stack = StackAllocator::Alloc(m_stacksize);

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT(false); // TODO sylar_assert2
        }
        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;

        if (!use_caller) {
            makecontext(&m_ctx, &Fiber::MainFunc, 0);
        } else {
            makecontext(&m_ctx, &Fiber::CallMainFunc, 0);
        }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
        if (!use_caller) {
            m_ctx = make_fcontext((char*)m_stack + m_stacksize, m_stacksize, &Fiber::MainFunc);
        } else {
            m_ctx = make_fcontext((char*)m_stack + m_stacksize, m_stacksize, &Fiber::CallMainFunc);
        }
#endif

        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber id: " << m_id; // Not use s_fiber_id
    }

    void Fiber::swapIn() {
        SetThis(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        if (swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
            SYLAR_ASSERT(false);
        }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
        jump_fcontext(&Scheduler::GetMainFiber()->m_ctx, m_ctx, 0);
#endif
    }

    void Fiber::swapOut() {
        SetThis(Scheduler::GetMainFiber());
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        if (swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)) {
                SYLAR_ASSERT(false);
        }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
        jump_fcontext(&m_ctx, Scheduler::GetMainFiber()->m_ctx, 0);
#endif
    }

    void Fiber::call() {
        SetThis(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        if (swapcontext(&t_main_thread_fiber->m_ctx, &m_ctx)) {
            SYLAR_ASSERT(false);
        }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
        jump_fcontext(&t_main_thread_fiber->m_ctx, m_ctx, 0);
#endif
    }

    void Fiber::back() {
        SetThis(t_main_thread_fiber.get());
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        if (swapcontext(&m_ctx, &t_main_thread_fiber->m_ctx)) {
            SYLAR_ASSERT(false);
        }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
        jump_fcontext(&m_ctx, t_main_thread_fiber->m_ctx, 0);
#endif
    }

    void Fiber::YeildToReady() { // Why need this
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur->m_state == EXEC);
        cur->m_state = READY;
        cur->swapOut();
    }

    void Fiber::YeildToHold() { // Normal used
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur->m_state == EXEC);
        cur->m_state = HOLD;
        cur->swapOut();
        //std::cout<< "YeildToHold cur->use_count(): " << cur.use_count() << std::endl;
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
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT("getcontext");
        }

        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;

        makecontext(&m_ctx, &Fiber::MainFunc, 0);
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
        m_ctx = make_fcontext((char*)m_stack + m_stacksize, m_stacksize, &Fiber::MainFunc);
#endif
        m_state = INIT;
    }

    uint64_t Fiber::TotalFibers() {
        return s_fiber_count;
    }

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    void Fiber::MainFunc() {
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    void Fiber::MainFunc(intptr_t vp) {
#endif
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

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    void Fiber::CallMainFunc() {
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    void Fiber::CallMainFunc(intptr_t vp) {
#endif
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
        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr->back();
        SYLAR_ASSERT(false);
    }

}
