#include "timer.hh"
#include "util.hh"

namespace sylar {

    bool Timer::Comparator::operator() (const Timer::ptr lhs, 
          const Timer::ptr rhs) const {
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
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end()) {
            return false; 
        }
        m_manager->m_timers.erase(it);
        m_next = sylar::GetCurrentMs() + m_Tms;
        m_manager->m_timers.insert(shared_from_this());
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
            start = m_next - mTms;
        }
        m_Tms = ms;
        m_next = m_Tms + start;
        m_manager->addTimer(shared_from_this(), lock); // Why not insert directly?
        return true;
    }

    TimerManager::TimerManager() {
    }

    TimerManager::~TimerManager() {
    }

    void TimerManager::addTimer(Timer::ptr timer, const RWMutexType::WriteLock& m) {
        auto it = m_timer.insert(timer).first; //迭代器
        bool at_front = (it == m_timer.begin()); //堆顶最小值
        if (at_front) {
            m_tickle = true;
        }
        lock.unlock();
        if (at_front) {
            //
        }
    }

    Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recuring = false) {
        Timer::ptr timer(new Timer(ms, cb, recuring, this)); // Why not shared_from_this
        RWMutexType::WriteLock lock(m_mutex);
        addTimer(timer, lock);
        return timer;
    }

    void TimerManager::listExpiresCbs(std::vector<std::function<void()> >& cbs) { // 核心函数 idle里每次都要执行
        uint64_t now_ms = sylar::GetCurrentMs();
        std::vector<Timer::ptr> expired;
        {
            RWMutexType::ReadLock lock(m_mutex);
            if (m_timers.empty()) {
                return;
            }
            Timer::ptr now_timer(new Timer(now_ms));
            auto it = m_timers.lower_bound(now_timer);
            expired.insert(expired.begin(), m_timers.begin(), it);
            m_timers.erase(m_timer.begin(), it);
            cbs.reserve(expired.size());
            for (const auto& it : expired) {
                cbs.push_back(it->m_cb);
                if (it->recursor) {
                    it->m_next = now_ms + it->m_ms;
                    m_timers.insert(it);
                } else {
                    it->m_cb = nullptr;  // Why
                }
            }
        }
    }

}
