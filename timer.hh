#ifndef __TIMER_HH__
#define __TIMER_HH__

#include <memory>
#include <functional>
#include <set>
#include <vector>

#include "thread.hh"

namespace sylar {

    class TimerManager;

    class Timer : public std::enable_shared_from_this<Timer> {
    friend class TimerManager;
    public:
        typedef std::shared_ptr<Timer> ptr;
        bool cancel();
        bool refresh();
        bool reset(uint64_t ms, bool from_now);

    private:
        struct Comparator {
            bool operator() (const Timer::ptr& l, const Timer::ptr& r) const;
        };
        Timer(uint64_t ms, std::function<void()> cb, bool recuring, TimerManager* manager);
        Timer(uint64_t next);
        uint64_t                m_Tms = 0;
        std::function<void()>   m_cb;
        bool                    m_recursor = false;
        uint64_t                m_next = 0;
        //TimerManager::ptr       m_manager = nullptr; // 作用是为了cancel refresh reset 直接从全局set上取下来? //前向引用 用不了
        TimerManager*       m_manager = nullptr; // 作用是为了cancel refresh reset 直接从全局set上取下来?
    };

    // Timer只包装时间变量 manager管理全局timers manager用在idle里 为了提升效率 里面肯定关联容器 关联容器就少不了仿函数/函数对象/谓词
    // 注意 这里是全源码第一处第一次用关联容器

    class TimerManager { // 模式: manager模式 对timers统计 // 标准的观察者模式
    friend class Timer;
    public:
        typedef std::shared_ptr<TimerManager> ptr;
        typedef RWMutex RWMutexType;

        TimerManager();
        virtual ~TimerManager();
        void addTimer(Timer::ptr timer, RWMutexType::WriteLock& m);
        Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recuring = false);
        Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb,
                std::weak_ptr<void> weak_cond, bool recuring = false);
        void listExpiresCbs(std::vector<std::function<void()> >& cbs);
        bool hasTimer();
        uint64_t getNextTimer();
    protected:
        virtual void onTimerInsertedAtFront() = 0;

    private:
        RWMutexType                             m_mutex;
        std::set<Timer::ptr, Timer::Comparator> m_timers;
        bool                                    m_tickle = false;
    };
}

#endif
