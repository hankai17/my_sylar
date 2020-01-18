#ifndef __TIMER_HH__
#define __TIMER_HH__

#include <memory>
#include <functional>

namespace sylar {
    class Timer {
    public:
        typedef std::shared_ptr<Timer> ptr;
        bool cancel();
        bool refresh();
        bool reset(uint64_t ms, bool from_now);

    private:
        Timer(uint64_t ms, std::function<void()> cb, bool recuring);
        Timer(uint64_t next);
        uint64_t                m_Tms = 0;
        std::function<void()>   m_cb;
        bool                    m_recursor = false;
        uint64_t                m_next = 0;
    };
}

#endif
