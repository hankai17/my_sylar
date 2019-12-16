#ifndef __THREAD_HH__
#define __THREAD_HH__

#include <memory>
#include <string>
#include <thread>
#include <functional>
#include <pthread.h>
#include <semaphore.h>
#include <bits/pthreadtypes.h>

namespace sylar {
    class Semaphore {
    public:
        Semaphore(uint32_t count = 0);
        ~Semaphore();

        void wait();
        void notify();

    private:
        Semaphore(const Semaphore&) = delete;
        Semaphore& operator= (const Semaphore&) = delete;
        Semaphore(const Semaphore&&) = delete;
    private:
        sem_t       m_semaphore;
    };

    template <typename T>
    class ScopedLockImpl {
    public:
        ScopedLockImpl(T& mutex)
        : m_mutex(mutex) {
            m_mutex.lock();
            m_locked = true;
        }

        ~ScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.lock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T&          m_mutex;
        bool        m_locked;
    };

    template <typename T>
    struct ReadScopedLockImpl {
    public:
        ReadScopedLockImpl(T& mutex)
        : m_mutex(mutex) {
            m_mutex.rdlock();
            m_locked = true;
        }

        ~ReadScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.rdlock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T&          m_mutex;
        bool        m_locked;
    };

    template <typename T>
    struct WriteScopedLockImpl {
    public:
        WriteScopedLockImpl(T& mutex)
                : m_mutex(mutex) {
            m_mutex.wrlock();
            m_locked = true;
        }

        ~WriteScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.wrlock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T&          m_mutex;
        bool        m_locked;
    };

    class RWMutex {
    public:
        typedef ReadScopedLockImpl<RWMutex> ReadLock;
        typedef WriteScopedLockImpl<RWMutex> WriteLock;

        RWMutex() {
            pthread_rwlock_init(&m_lock, nullptr);
        }

        ~RWMutex() {
            pthread_rwlock_destroy(&m_lock);
        }

        void rdlock() {
            pthread_rwlock_rdlock(&m_lock);
        }

        void wrlock() {
            pthread_rwlock_wrlock(&m_lock);
        }

        void unlock() {
            pthread_rwlock_unlock(&m_lock);
        }

    private:
        pthread_rwlock_t        m_lock;
    };

    class Thread {
    public:
        typedef std::shared_ptr<Thread> ptr;
        Thread(std::function<void()> cb, const std::string& name);
        ~Thread();

        pid_t getId() const { return m_id; }
        void setName(const std::string& name);
        const std::string& getName() const { return m_name; }
        static void* run(void* arg); // Why static ?
        static std::string& GetName(); // const { return t_thread_name; }
        static Thread* GetThis();

        void join();

    private:
        pid_t                   m_id = -1;
        std::function<void()>   m_cb;
        std::string             m_name;
        pthread_t               m_thread = 0;
        Semaphore               m_semphore;
    };
}

#endif
