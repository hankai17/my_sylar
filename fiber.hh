#ifndef __FIBER__HH__
#define __FIBER__HH__

#include <memory>
#include <functional>

#define FIBER_UCONTEXT 1
#define FIBER_FCONTEXT 2
#define FIBER_CONTEXT_TYPE FIBER_FCONTEXT
//#define FIBER_CONTEXT_TYPE FIBER_UCONTEXT

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
#include "ucontext.h"
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
#include "fcontext/fcontext.hh"
#endif

namespace sylar {
    class Fiber : public std::enable_shared_from_this<Fiber> {
    public:
        friend class Scheduler;
        typedef std::shared_ptr<Fiber> ptr;
        enum State {
            INIT,
            HOLD,
            EXEC,
            TERM,
            READY,
            EXCEPT
        };

        Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);
        ~Fiber();
        void reset(std::function<void()> cb);
        void swapIn();
        void swapOut();
        void call();
        void back();
        static void SetThis(Fiber* f);
        static Fiber::ptr GetThis();
        static void YeildToReady();
        static void YeildToHold();
        static uint64_t TotalFibers();

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        static void MainFunc();
        static void CallMainFunc();
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
        static void MainFunc(intptr_t vp);
        static void CallMainFunc(intptr_t vp);
#endif
        uint64_t getFiberId() const { return m_id; }
        static uint64_t GetFiberId();
        void setState(State state) { m_state = state; }
        State getState() const { return m_state; }

    private:
        Fiber(); // Why private?

        uint64_t m_id = 0;
        uint32_t m_stacksize = 0;
        State m_state = INIT;
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
        ucontext_t m_ctx;
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
        fcontext_t m_ctx = nullptr;
#endif
        void *m_stack = nullptr;
        std::function<void()> m_cb;
    };
}

#endif