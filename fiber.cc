#include "fiber.hh"
#include "macro.hh"
#include "log.hh"
#include "config.hh"
#include <atomic>

namespace sylar {
    static thread_local Fiber *t_fiber = nullptr;
    static std::atomic<uint64_t> s_fiber_count {0};
    static std::atomic<uint64_t> s_fiber_id {0};

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static sylar::ConfigVar<uint32_t>::ptr g_fiber_stack_size =
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

    Fiber::ptr GetThis() {
        return nullptr;
    }

    Fiber::Fiber() {
        m_state = EXEC;
        SetThis(this);
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT(false); // TODO sylar_assert2
        }
        ++s_fiber_count;
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber";
    }

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
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber id: " << m_id;
    }


    void Fiber::reset(std::function<void()> cb) {

    }

    void Fiber::swapIn() {
        SetThis(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
        //swapcontext(& &m_ctx)
    }

    void Fiber::swapOut() {

    }

    void Fiber::YeildToReady() {

    }

    void Fiber::YeildToHold() {

    }

    uint64_t Fiber::TotalFibers() {
        return 0;
    }

    void Fiber::MainFunc() {

    }


}