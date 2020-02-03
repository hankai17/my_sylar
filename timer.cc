#include "timer.hh"
#include "util.hh"

namespace sylar {

    bool Timer::Comparator::operator() (const Timer::ptr& lhs, 
          const Timer::ptr& rhs) const {
        if (!lhs && !rhs) {
            return false;
        }
        if (!lhs) {
            return true; // 左小右大 即从小到大排序
        }
        if (!rhs) {
            return false;
        }
        if (lhs->m_next < rhs->m_next) {
            return true;
        }
        if (lhs->m_next > rhs->m_next) {
            return false;
        }
        //return lhs.get() < rhs.get(); // Why
        return false;
    }

    Timer::Timer(uint64_t ms, std::function<void()> cb, bool recuring, TimerManager* manager)
    : m_Tms(ms),
    m_cb(cb),
    m_recursor(recuring),
    m_manager(manager) {
        m_next = sylar::GetCurrentMs() + m_Tms;
    }

    Timer::Timer(uint64_t next)
    : m_next(next) {
    }

    bool Timer::cancel() {
        TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
        if (m_cb) {
            m_cb = nullptr;
            auto it = m_manager->m_timers.find(shared_from_this());
            if (it == m_manager->m_timers.end()) {
                return false;
            }
            m_manager->m_timers.erase(it);
            return true;
        }
        return false;
    }

    bool Timer::refresh() {
        TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
        if (!m_cb) {
            return false;
        }
        auto it = m_manager->m_timers.find(shared_from_this()); // 首先timers里得有 即针对的仍然是周期任务
        if (it == m_manager->m_timers.end()) {
            return false; 
        }
        m_manager->m_timers.erase(it);
        m_next = sylar::GetCurrentMs() + m_Tms; // 续命周期?
        m_manager->m_timers.insert(shared_from_this()); //不能直接改 必须erase后再插入 //好哲学
        return true;
    }

    bool Timer::reset(uint64_t ms, bool from_now) {
        if (ms == m_Tms && !from_now) {
            return true;
        }
        TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
        if (!m_cb) {
            return false;
        }
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end()) {
            return false;
        }
        m_manager->m_timers.erase(it);
        uint64_t start = 0;
        if (from_now) {
            start = sylar::GetCurrentMs();
        } else {
            start = m_next - m_Tms;
        }
        m_Tms = ms;
        m_next = m_Tms + start;
        m_manager->addTimer(shared_from_this(), lock); // Why not insert directly? //感觉可以直接插入
        return true;
    }

    TimerManager::TimerManager() {
    }

    TimerManager::~TimerManager() {
    }

    bool TimerManager::hasTimer() {
        RWMutexType::ReadLock lock(m_mutex);
        return !m_timers.empty();
    }

    uint64_t TimerManager::getNextTimer() { //现在距离堆头还有多长时间 用在idle里stop那里 epoll timeout
        RWMutexType::ReadLock lock(m_mutex);
        m_tickle = false; // Why
        if (m_timers.empty()) {
            return ~0ull; //FFFFFFFF
        }

        const Timer::ptr& next = *m_timers.begin();
        uint64_t now_ms = sylar::GetCurrentMs();
        if (now_ms >= next->m_next) {
            return 0;
        } else {
            return next->m_next - now_ms;
        }
    }

    void TimerManager::addTimer(Timer::ptr timer, RWMutexType::WriteLock& lock) { //base
        auto it = m_timers.insert(timer).first; //迭代器
        bool at_front = (it == m_timers.begin()); //堆顶最小值
        if (at_front) {
            m_tickle = true;
        }
        lock.unlock();
        if (at_front) {
            onTimerInsertedAtFront();
        }
    }

    Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recuring) { // 外部directly使用
        Timer::ptr timer(new Timer(ms, cb, recuring, this)); // Why not shared_from_this
        RWMutexType::WriteLock lock(m_mutex);
        addTimer(timer, lock);
        return timer;
    }

    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
        std::shared_ptr<void> tmp = weak_cond.lock();
        if (tmp) {
            cb();
        }
    }

    Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb,
                                 std::weak_ptr<void> weak_cond, bool recuring) {
        return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recuring);
    }

    void TimerManager::listExpiresCbs(std::vector<std::function<void()> >& cbs) { // 核心函数 idle里每次都要执行
        uint64_t now_ms = sylar::GetCurrentMs();
        std::vector<Timer::ptr> expired;
        {
            RWMutexType::ReadLock lock(m_mutex);
            if (m_timers.empty()) {
                return;
            }
        }

        RWMutexType::WriteLock lock(m_mutex);
        //暂不考虑更改系统问题
        Timer::ptr now_timer(new Timer(now_ms));
        auto it = m_timers.lower_bound(now_timer);
        expired.insert(expired.begin(), m_timers.begin(), it);
        m_timers.erase(m_timers.begin(), it);
        cbs.reserve(expired.size());
        for (const auto& it : expired) {
            cbs.push_back(it->m_cb);
            if (it->m_recursor) {
                it->m_next = now_ms + it->m_Tms;
                m_timers.insert(it);
            } else {
                it->m_cb = nullptr;  // Why
            }
        }
    }

}
