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
                    m_cb(back.first, back.sencond);
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
        map_type        m_cache
        list_type       m_keys;
        size_t          m_maxSize;
        size_t          m_elasticity;
        prune_callback  m_cb;
        CacheStatus*    m_status = nullptr;
        bool            m_statusOwner = false;
    };

    template <typename K, typename V, typename MutexType = sylar::Mutex, typename Hash = std::hash<K> >
            class HashLruCache {
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
                        m_datas[i] = new cache_type(pre_max_size, pre_elasticity, &m_Status);
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
}

#endif