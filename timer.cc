#include "timer.hh"
#include "util.hh"

namespace sylar {

    Timer::Timer(uint64_t ms, std::function<void()> cb, bool recuring)
    : m_Tms(ms),
    m_cb(cb),
    m_recursor(recuring) {
        m_next = sylar::GetCurrentMs() + m_Tms;
    }

    Timer::Timer(uint64_t next)
    : m_next(next) {
    }

    bool Timer::cancel() {
        if (m_cb) {
            m_cb = nullptr;
            return true;
        }
        return false;
    }

    bool Timer::refresh() {
        if (!m_cb) {
            return false;
        }
        m_next = sylar::GetCurrentMs() + m_Tms;
        return true;
    }

    bool Timer::reset(uint64_t ms, bool from_now) {
        if (ms == m_Tms && !from_now) {
            return true;
        }
        if (!m_cb) {
            return false;
        }
        uint64_t start = sylar::GetCurrentMs();
        m_Tms = ms;
        m_next = m_Tms + start;
        return true;
    }

}