#ifndef __LRU_HH__
#define __LRU_HH__

#include <memory>
#include <list>
#include <map>
#include <unordered_map>
#include <functional>
#include <vector>
#include "my_sylar/util.hh"
#include "my_sylar/thread.hh"

namespace sylar {
    class CacheStatus {
    public:
        CacheStatus() {}
        int64_t incGet(int64_t v = 1) { return Atomic::addFetch(m_get, v); }
        int64_t incSet(int64_t v = 1) { return Atomic::addFetch(m_set, v); }
        int64_t incDel(int64_t v = 1) { return Atomic::addFetch(m_del, v); }
        int64_t incTimeout(int64_t v = 1) { return Atomic::addFetch(m_timeout, v); }
        int64_t incPrune(int64_t v = 1) { return Atomic::addFetch(m_prune, v); }
        int64_t incHit(int64_t v = 1) { return Atomic::addFetch(m_hit, v); }

        int64_t decGet(int64_t v = 1) { return Atomic::subFetch(m_get, v); }
        int64_t decSet(int64_t v = 1) { return Atomic::subFetch(m_set, v); }
        int64_t decDel(int64_t v = 1) { return Atomic::subFetch(m_del, v); }
        int64_t decTimeout(int64_t v = 1) { return Atomic::subFetch(m_timeout, v); }
        int64_t decPrune(int64_t v = 1) { return Atomic::subFetch(m_prune, v); }
        int64_t decHit(int64_t v = 1) { return Atomic::subFetch(m_hit, v); }

        int64_t getGet() const { return m_get; }
        int64_t getSet() const { return m_set; }
        int64_t getDel() const { return m_del; }
        int64_t getTimeout() const { return m_timeout; }
        int64_t getPrune() const { return m_prune; }
        int64_t getHit() const { return m_hit; }

        double getHitRate() const {
            return m_get ? (m_hit * 1.0 / m_get) : 0;
        }

        void merge(const CacheStatus& o) {
            m_get += o.m_get;
            m_set += o.m_set;
            m_del += o.m_del;
            m_timeout += o.m_timeout;
            m_prune += o.m_prune;
            m_hit += o.m_hit;
        }

        std::string toString() const {
            std::stringstream ss;
            ss << "get: " << m_get
            << " set: " << m_set
            << " del: " << m_del
            << " timeout: " << m_timeout
            << " prune: " << m_prune
            << " hit: " << m_hit
            << " hit_rate: " << (getHitRate() * 100.0) << "%";
            return ss.str();
        }

    private:
        int64_t         m_get = 0;
        int64_t         m_set = 0;
        int64_t         m_del = 0;
        int64_t         m_timeout = 0;
        int64_t         m_prune = 0;
        int64_t         m_hit = 0;
    };

    template <typename K, typename V, typename MutexType = sylar::Mutex>
    class LruCache {
    public:
        typedef std::shared_ptr<LruCache> ptr;
        typedef std::pair<K, V> item_type;
        typedef std::list<item_type> list_type;
        typedef std::unordered_map<K, typename list_type::iterator> map_type;
        typedef std::function<void(const K&, const V&)> prune_callback;

        LruCache(size_t max_size = 0, size_t elasticity = 0,
                CacheStatus* status = nullptr) :
                m_maxSize(max_size),
                m_elasticity(elasticity) {
            m_status = status;
            if (m_status == nullptr) {
                m_status = new CacheStatus;
                m_statusOwner = true;
            }
        }

        ~LruCache() {
            if (m_statusOwner && m_status) {
                delete m_status;
            }
        }

        void set(const K& k, const V& v) {
            m_status->incSet();
            typename MutexType::Lock lock(m_mutex);
            auto it = m_cache.find(k);
            if (it != m_cache.end()) {
                it->second->second = v;
                m_keys.splice(m_keys.begin(), m_keys, it->second);
                return;
            }
            m_keys.emplace_front(std::make_pair(k, v));
            m_cache.insert(std::make_pair(k, m_keys.begin()));
            prune();
        }

        bool get(const K& k, V& v) {
            m_status->incGet();
            typename MutexType::Lock lock(m_mutex);
            auto it = m_cache.find(k);
            if (it == m_cache.end()) {
                return false;
            }
            m_keys.splice(m_keys.begin(), m_keys, it->second);
            v = it->second->second;
            lock.unlock();
            m_status->incHit();
            return true;
        }

        V get(const K& k) {
            m_status->incGet();
            typename MutexType::Lock lock(m_mutex);
            auto it = m_cache.find(k);
            if (it == m_cache.end()) {
                return V();
            }
            m_keys.splice(m_keys.begin(), m_keys, it->second);
            auto v = it->second->second;
            lock.unlock();
            m_status->incHit();
            return v;
        }

        bool del(const K& k) {
            m_status->incDel();
            typename MutexType::Lock lock(m_mutex);
            auto it = m_cache.find(k);
            if (it == m_cache.end()) {
                return false;
            }
            m_keys.erase(it->second);
            m_cache.erase(it);
            return true;
        }

        bool exists(const K& k) {
            typename MutexType::Lock lock(m_mutex);
            return m_cache.find(k) != m_cache.end();
        }

        size_t size() {
            typename MutexType::Lock lock(m_mutex);
            return m_cache.size();
        }

        bool empty() {
            typename MutexType::Lock lock(m_mutex);
            return m_cache.empty();
        }

        bool clear() {
            typename MutexType::Lock lock(m_mutex);
            m_cache.clear();
            m_keys.clear();
            return true;
        }

        void setMaxSize(const size_t& v) { m_maxSize = v; }
        void setElasticity(const size_t& v) { m_elasticity = v; }
        size_t getMaxSize() const { return m_maxSize; }
        size_t getElasticity() const { return m_elasticity; }
        size_t getMaxAllowedSize() const { return m_maxSize + m_elasticity; }

        template <typename F>
        void foreach(F& f) {
            typename MutexType::Lock lock(m_mutex);
            std::for_each(m_cache.begin(), m_cache.end(), f);
        }

        void setPruneCallback(prune_callback cb) { m_cb = cb; }
        std::string toStatusString() {
            std::stringstream ss;
            ss << (m_status ? m_status->toString() : "no status")
            << " total: " << size();
            return ss.str();
        }

        CacheStatus* getStatus() const { return m_status; }
        void setStatus(CacheStatus* v, bool owner = false) {
            if (m_statusOwner && m_status) {
                delete m_status;
            }
            m_status = v;
            m_statusOwner = owner;
            if (m_status == nullptr) {
                m_status = new CacheStatus;
                m_statusOwner = true;
            }
        }

    protected:
        size_t prune() {
            if (m_maxSize == 0 || m_cache.size() < getMaxAllowedSize()) {
                return 0;
            }
            size_t count = 0;
            while (m_cache.size() > m_maxSize) {
                auto& back = m_keys.back();
                if (m_cb) {
                    m_cb(back.first, back.second);
                }
                m_cache.erase(back.first);
                m_keys.pop_back();
                ++count;
            }
            m_status->incPrune(count);
            return count;
        }

    private:
        MutexType       m_mutex;
        map_type        m_cache;
        list_type       m_keys;
        size_t          m_maxSize;
        size_t          m_elasticity;
        prune_callback  m_cb;
        CacheStatus*    m_status = nullptr;
        bool            m_statusOwner = false;
    };

    template <typename K, typename V, typename MutexType = sylar::Mutex, typename Hash = std::hash<K> >
            class HashLruCache { // LRU just a array. HashLru is a bucket. To reduce collisions ?
            public:
                typedef std::shared_ptr<HashLruCache> ptr;
                typedef LruCache<K, V, MutexType> cache_type;

                HashLruCache(size_t bucket, size_t max_size, size_t elasticity) :
                m_bucket(bucket) {
                    m_datas.resize(bucket);
                    size_t pre_max_size = std::ceil(max_size * 1.0 / bucket);
                    size_t pre_elasticity = std::ceil(elasticity * 1.0 /bucket);
                    m_maxSize = pre_max_size * bucket;
                    for (size_t i = 0; i < bucket; i++) {
                        m_datas[i] = new cache_type(pre_max_size, pre_elasticity, &m_status);
                    }
                }

                ~HashLruCache() {
                    for (size_t i = 0; i < m_datas.size(); i++) {
                        delete m_datas[i];
                    }
                }

                void set(const K& k, const V& v) {
                    m_datas[m_hash(k) % m_bucket]->set(k, v);
                }

                bool get(const K& k, V& v) {
                    return m_datas[m_hash(k) % m_bucket]->get(k, v);
                }

                V get(const K& k) {
                    return m_datas[m_hash(k) % m_bucket]->get(k);
                }

                bool del(const K& k) {
                    return m_datas[m_hash(k) % m_bucket]->del(k);
                }

                bool exists(const K& k) {
                    return m_datas[m_hash(k) % m_bucket]->exists(k);
                }

                size_t size() {
                    size_t total = 0;
                    for (auto& i : m_datas) {
                        total += i->size();
                    }
                    return total;
                }

                bool empty() {
                    for (auto& i : m_datas) {
                        if (!i->empty()) {
                            return false;
                        }
                    }
                    return true;
                }

                void clear() {
                    for (auto& i : m_datas) {
                        i->clear();
                    }
                }

                size_t getMaxSize() const { return m_maxSize; }
                size_t getElasticity() const { return m_elasticity; }
                size_t getMaxAllowedSize() const { return m_maxSize + m_elasticity; }
                size_t getBucket() const { return m_bucket; }

                void setMaxSize(const size_t& v) {
                    size_t pre_max_size = std::ceil(v * 1.0 / m_bucket);
                    m_maxSize = pre_max_size * m_bucket;
                    for (auto&i : m_datas) {
                        i->setMaxSize(pre_max_size);
                    }
                }

                void setElasticity(const size_t& v) {
                    size_t pre_elasticity = std::ceil(v * 1.0 / m_bucket);
                    m_elasticity = pre_elasticity * m_bucket;
                    for (auto& i : m_datas) {
                        i->setElasticity(pre_elasticity);
                    }
                }

                template <typename F>
                void foreach(F& f) {
                    for (auto& i : m_datas) {
                        i->foreach(f);
                    }
                }

                void setPruneCallback(typename cache_type::prune_callback cb) {
                    for (auto& i : m_datas) {
                        i->setPruneCallback(cb);
                    }
                }

                CacheStatus* getStatus() {
                    return &m_status;
                }

                std::string toStatusString() {
                    std::stringstream ss;
                    ss << m_status.toString() << " total: "
                    << size();
                    return ss.str();
                }

            private:
                std::vector<cache_type*>        m_datas;
                size_t                          m_maxSize;
                size_t                          m_bucket;
                size_t                          m_elasticity;
                Hash                            m_hash;
                CacheStatus                     m_status;
            };

    template <typename K, typename V, typename RWMutexType = sylar::RWMutex>
            class TimedCache {
            private:
                struct Item {
                    Item(const K& k, const V& v, const uint64_t& t) :
                    key(k), val(v), ts(t) {}
                    K           key;
                    V           val; // mutable V   val;
                    uint64_t    ts;

                    bool operator< (const Item& oth) const {
                        if (ts != oth.ts) {
                            return ts < oth.ts;
                        }
                        return key < oth.key;
                    }
                };

            public:
                typedef std::shared_ptr<TimedCache> ptr;
                typedef Item item_type;
                typedef std::set<item_type> set_type;
                typedef std::unordered_map<K, typename set_type::iterator> map_type;
                typedef std::function<void(const K&, const V&)> prune_callback;

                TimedCache(size_t max_size = 0, size_t elasticity = 0,
                        CacheStatus* status = nullptr) :
                        m_maxSize(max_size),
                        m_elasticity(elasticity),
                        m_status(status) {
                    if (m_status == nullptr) {
                        m_status = new CacheStatus;
                        m_statusOwner = true;
                    }
                }

                ~TimedCache() {
                    if (m_statusOwner && m_status) {
                        delete m_status;
                    }
                }

                void set(const K& k, const V& v, uint64_t expired) {
                    m_status->incSet();
                    typename RWMutexType::WriteLock loc(m_mutex);
                    auto it = m_cache.find(k);
                    if (it != m_cache.end()) {
                        m_cache.erase(it);
                    }
                    auto set_it = m_timed.insert(Item(k, v, expired + sylar::GetCurrentMs()));
                    m_cache.insert(std::make_pair(k, set_it.first));
                    prune();
                }

                V get(const K& k) {
                    m_status->incGet();
                    typename RWMutexType::ReadLock lock(m_mutex);
                    auto it = m_cache.find(k);
                    if (it == m_cache.end()) {
                        return V();
                    }
                    auto v = it->second->val;
                    lock.unlock();
                    m_status->incHit();
                    return v;
                }

                bool get(const K& k, V& v) {
                    m_status->incGet();
                    typename RWMutexType::ReadLock lock(m_mutex);
                    auto it = m_cache.find(k);
                    if (it == m_cache.end()) {
                        return false;
                    }
                    v = it->second->val;
                    lock.unlock();
                    m_status->incHit();
                    return true;
                }

                bool del(const K& k) {
                    m_status->incDel();
                    typename RWMutexType::WriteLock lock(m_mutex);
                    auto it = m_cache.find(k);
                    if (it == m_cache.end()) {
                        return false;
                    }
                    m_timed.erase(it->second);
                    m_cache.erase(it);
                    lock.unlock();
                    //m_status->incHit();
                }

                bool expired(const K& k, const uint64_t& ts) {
                    typename RWMutexType::WriteLock lock(m_mutex);
                    auto it = m_cache.find(k);
                    if (it == m_cache.end()) {
                        return false;
                    }
                    uint64_t tts = ts + sylar::GetCurrentMs();
                    if (it->second->ts == tts) {
                        return true;
                    }
                    auto item = *it->second;
                    m_timed.erase(it->second);
                    auto iit = m_timed.insert(item);
                    it->second = iit.first;
                    return true;
                }

                bool exists(const K& k) {
                    typename RWMutexType::ReadLock lock(m_mutex);
                    return m_cache.find(k) != m_cache.end();
                }

                size_t size() {
                    typename RWMutexType::ReadLock lock(m_mutex);
                    return m_cache.size();
                }

                size_t empty() {
                    typename RWMutexType::ReadLock lock(m_mutex);
                    return m_cache.empty();
                }

                bool clear() {
                    typename RWMutexType::WriteLock lock(m_mutex);
                    m_timed.clear();
                    m_cache.clear();
                    return true;
                }

                void setMaxSize(const size_t& v) { m_maxSize = v; }
                void setElasticity(const size_t& v) { m_elasticity = v; }
                size_t getMaxSize() const { return m_maxSize; }
                size_t getElasticity() const { return m_elasticity; }
                size_t getMaxAllowedSize() const { return m_maxSize + m_elasticity; }
                void setPruneCallback(prune_callback cb) { m_cb = cb; }

                template <typename F>
                void foreach(F& f) {
                    typename RWMutexType::ReadLock lock(m_mutex);
                    std::for_each(m_cache.begin(), m_cache.end(), f);
                }

                std::string toStatusString() {
                    std::stringstream ss;
                    ss << (m_status ? m_status->toString() : "no status")
                       << " total: " << size();
                    return ss.str();
                }

                CacheStatus* getStatus() const { return m_status; }
                void setStatus(CacheStatus* v, bool owner = false) {
                    if (m_statusOwner && m_status) {
                        delete m_status;
                    }
                    m_status = v;
                    m_statusOwner = owner;
                    if (m_status == nullptr) {
                        m_status = new CacheStatus;
                        m_statusOwner = true;
                    }
                }

                size_t checkTimeout(const uint64_t& ts = sylar::GetCurrentMs()) {
                    size_t size = 0;
                    typename RWMutexType::WriteLock lock(m_mutex);
                    for (auto it = m_timed.begin(); it != m_timed.end();) {
                        if (it->ts <= ts) {
                            if (m_cb) {
                                m_cb(it->key, it->val);
                            }
                            m_cache.erase(it->key);
                            m_timed.erase(it++);
                            ++size;
                        } else {
                            break;
                        }
                    }
                    return size;
                }

            protected:
                size_t prune() {
                    if (m_maxSize == 0 || m_cache.size() < getMaxAllowedSize()) {
                        return 0;
                    }
                    size_t count = 0;
                    while (m_cache.size() > m_maxSize) {
                        auto it = m_timed.begin();
                        if (m_cb) {
                            m_cb(it->key, it->val);
                        }
                        m_cache.erase(it->key);
                        m_timed.erase(it);
                        ++count;
                    }
                    m_status->incPrune();
                    return count;
                }

            private:
                RWMutexType         m_mutex;
                uint64_t            m_maxSize;
                uint64_t            m_elasticity;
                CacheStatus*        m_status;
                set_type            m_timed;
                map_type            m_cache;
                prune_callback      m_cb;
                bool                m_statusOwner = false;
            };

    template <typename K, typename V, typename RWMutexType = sylar::RWMutex,
            typename Hash = std::hash<K> >
            class HashTimedCache {
            public:
                typedef std::shared_ptr<HashTimedCache> ptr;
                typedef TimedCache<K, V, RWMutexType> cache_type;
                HashTimedCache(size_t bucket, size_t max_size, size_t elasticity) :
                m_bucket(bucket) {
                    m_datas.resize(bucket);
                    size_t pre_max_size = std::ceil(max_size * 1.0 / bucket);
                    size_t pre_elasticity = std::ceil(elasticity * 1.0 / bucket);
                    m_maxSize = pre_max_size * bucket;
                    m_elasticity = pre_elasticity * bucket;
                    for (size_t i = 0; i < bucket; i++) {
                        m_datas[i] = new cache_type(pre_max_size,
                                pre_elasticity, &m_status);
                    }
                }

                ~HashTimedCache() {
                    for (size_t i = 0; i < m_datas.size(); i++) {
                        delete m_datas[i];
                    }
                }

                void set(const K& k, const V& v, uint64_t expired) {
                    m_datas[m_hash(k) % m_bucket]->set(k, v, expired);
                }

                bool expired(const K& k, uint64_t expired) {
                    return m_datas[m_hash(k) % m_bucket]->expired(k, expired);
                }

                bool get(const K& k, V& v) {
                    return m_datas[m_hash(k) % m_bucket]->get(k, v);
                }

                V get(const K& k) {
                    return m_datas[m_hash(k) % m_bucket]->get(k);
                }

                bool del(const K& k) {
                    return m_datas[m_hash(k) % m_bucket]->del(k);
                }

                bool exists(const K& k) {
                    return m_datas[m_hash(k) % m_bucket]->exists(k);
                }

                size_t size() {
                    size_t total = 0;
                    for (auto& i : m_datas) {
                        total += i->size();
                    }
                    return total;
                }

                bool empty() {
                    for (auto& i : m_datas) {
                        if (!i->empty()) {
                            return false;
                        }
                    }
                    return true;
                }

                void clear() {
                    for (auto& i : m_datas) {
                        i->clear();
                    }
                }

                size_t getMaxSize() const { return m_maxSize; }
                size_t getElasticity() const { return m_elasticity; }
                size_t getMaxAllowedSize() const { return m_maxSize + m_elasticity; }
                size_t getBucket() const { return m_bucket; }

                void setMaxSize(const size_t& v) {
                    size_t pre_max_size = std::ceil(v * 1.0 / m_bucket);
                    m_maxSize = pre_max_size * m_bucket;
                    for (auto&i : m_datas) {
                        i->setMaxSize(pre_max_size);
                    }
                }

                void setElasticity(const size_t& v) {
                    size_t pre_elasticity = std::ceil(v * 1.0 / m_bucket);
                    m_elasticity = pre_elasticity * m_bucket;
                    for (auto& i : m_datas) {
                        i->setElasticity(pre_elasticity);
                    }
                }

                template <typename F>
                void foreach(F& f) {
                    for (auto& i : m_datas) {
                        i->foreach(f);
                    }
                }

                void setPruneCallback(typename cache_type::prune_callback cb) {
                    for (auto& i : m_datas) {
                        i->setPruneCallback(cb);
                    }
                }

                CacheStatus* getStatus() {
                    return &m_status;
                }

                std::string toStatusString() {
                    std::stringstream ss;
                    ss << m_status.toString() << " total: "
                       << size();
                    return ss.str();
                }

                size_t checkTimeout(const uint64_t& ts = sylar::GetCurrentMs()) {
                    size_t size = 0;
                    for (const auto& i : m_datas) {
                        size += i->checkTimeout(ts);
                    }
                    return size;
                }

            private:
                std::vector<cache_type*>        m_datas;
                size_t                          m_maxSize;
                size_t                          m_bucket;
                size_t                          m_elasticity;
                Hash                            m_hash;
                CacheStatus                     m_status;

            };

}

#endif