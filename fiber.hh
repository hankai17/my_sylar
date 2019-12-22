#ifndef __FIBER__HH__
#define __FIBER__HH__

#include <memory>
#include <functional>
#include <ucontext.h>

namespace sylar {
    class Fiber {
    public:
        typedef std::shared_ptr<Fiber> ptr;
        enum State {
            INIT,
            HOLD,
            EXEC,
            TERM,
            READY,
            EXCEPT
        };

        Fiber(std::function<void()> cb, size_t stacksize = 0);

        ~Fiber();

        void reset(std::function<void()> cb);

        void swapIn();

        void swapOut();

        static void SetThis(Fiber *f);

        static Fiber::ptr GetThis();

        static void YeildToReady();

        static void YeildToHold();

        static uint64_t TotalFibers();

        static void MainFunc();

    private:
        Fiber();

        uint64_t m_id = 0;
        uint32_t m_stacksize = 0;
        State m_state = INIT;
        ucontext_t m_ctx;
        void *m_stack = nullptr;
        std::function<void()> m_cb;
    };
}

#endif