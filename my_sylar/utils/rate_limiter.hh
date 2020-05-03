#ifndef __RATE_LIMITER_HH__
#define __RATE_LIMITER_HH__

#include <memory>
#include <list>
#include <map>

#include "my_sylar/timer.hh"
#include "my_sylar/thread.hh"
#include "my_sylar/util.hh"
#include "my_sylar/macro.hh"

namespace sylar {
    class RateLimiter {
    private:
        struct Bucket {
            Bucket() : m_count(0u) {}

            std::list<uint64_t> m_timestamps;
            size_t              m_count;
            Timer::ptr          m_timer;

        };

    public:
        RateLimiter(TimerManager& timerMgr, size_t countLimit,
                uint64_t timeLimit) :
                m_timerMgr(timerMgr),
                m_countLimit(countLimit),
                m_timeLimit(timeLimit) {}

        ~RateLimiter() {
            for (const auto& i : m_buckets) {
                if (i.second.m_timer) {
                    i.second.m_timer->cancel();
                }
            }
        }

        bool allowed(const int& key) {
            Mutex::Lock lock(m_mutex);
            uint64_t now = GetCurrentUs();
            Bucket& bucket = m_buckets[key];

            trim(bucket, now, m_countLimit, m_timeLimit); // 裁剪桶里过期元素 // 置空Bucket's timer
            if (bucket.m_count >= m_countLimit) {
                startTimer(key, bucket, now, m_timeLimit);
                return false;
            }
            bucket.m_timestamps.push_back(now);
            ++bucket.m_count;
            startTimer(key, bucket, now, m_timeLimit);
            SYLAR_ASSERT(bucket.m_count == bucket.m_timestamps.size());
            return true;
        }

    private:
        void drop(Bucket& bucket) {
            bucket.m_timestamps.pop_front();
            --bucket.m_count;
            if (bucket.m_timer) {
                bucket.m_timer->cancel();
                bucket.m_timer.reset();
            }
        }

        void trim(Bucket& bucket, uint64_t now, size_t countLimit,
                uint64_t timeLimit) {
            SYLAR_ASSERT(bucket.m_count == bucket.m_timestamps.size());
            while (!bucket.m_timestamps.empty() &&
                (bucket.m_timestamps.front() < now - timeLimit || bucket.m_count > countLimit)) { // 一般情况是桶里个数多 桶里的时间小于当前时间
                drop(bucket);
            }
            SYLAR_ASSERT(bucket.m_count == bucket.m_timestamps.size());
        }

        void trimeKey(const int& key) {
            Mutex::Lock loc(m_mutex);
            Bucket& bucket = m_buckets[key];
            uint64_t now = GetCurrentUs();
            trim(bucket, now, m_countLimit, m_timeLimit);
            startTimer(key, bucket, now, m_timeLimit);
        }

        void startTimer(const int& key, Bucket& bucket, uint64_t now, uint64_t timeLimit) {
            if (!bucket.m_timestamps.empty()) {
                if (!bucket.m_timer) {
                    bucket.m_timer = m_timerMgr.addTimer(bucket.m_timestamps.front() + timeLimit - now,
                            std::bind(&RateLimiter::trimeKey, this, key));
                }
            } else {
                m_buckets.erase(key);
            }
        }


    private:
        TimerManager&       m_timerMgr;
        size_t              m_countLimit;
        uint64_t            m_timeLimit;
        std::map<int, Bucket>   m_buckets;
        Mutex                   m_mutex;
    };
}


#endif