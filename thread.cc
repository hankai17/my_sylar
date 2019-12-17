#include "thread.hh"
#include "log.hh"
#include "util.hh"

namespace sylar {

    static thread_local Thread* t_thread = nullptr;
    static thread_local std::string t_thread_name = "UNKNOW";
    //static sylar::Logger::ptr logger = SYLAR_LOG_NAME("root");
    //static sylar::Logger::ptr logger = SYLAR_LOG_ROOT(); // Why not here

    Semaphore::Semaphore(uint32_t count) {
        if (sem_init(&m_semaphore, 0, count)) {
            throw std::logic_error("sem_init failed");
        }
    }

    Semaphore::~Semaphore() {
        sem_destroy(&m_semaphore);
    }

    void Semaphore::wait() {
        if (sem_wait(&m_semaphore)) {
            throw std::logic_error("sem_wait failed");
        }
    }

    void Semaphore::notify() {
        if (sem_post(&m_semaphore)) {
            throw std::logic_error("sem_wait failed");
        }
    }

    std::string& Thread::GetName() {
        return t_thread_name;
    }

    Thread* Thread::GetThis() { // Why pointer?
        return t_thread;
    }

    void* Thread::run(void* arg) {
        Thread* thread = (Thread*)arg;
        t_thread = thread;
        t_thread_name = thread->m_name;
        thread->m_id = sylar::GetThreadId();
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

        thread->m_semphore.notify(); // Why here
        thread->m_cb(); // Why use swap()
        return 0;
    }

    Thread::Thread(std::function<void()> cb, const std::string& name)
    : m_cb(cb),
    m_name(name) {
        if (name.empty()) {
            m_name = "UNKNOW";
        }
        int ret = pthread_create(&m_thread, nullptr, &Thread::run, this);
        if (ret) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "pthread create failed, ret = " << ret
            << " name = " << name;
            throw std::logic_error("pthread create failed");
        }
        m_semphore.wait();
    }

    void Thread::join() {
        if (m_thread) {
            int ret = pthread_join(m_thread, nullptr);
            if (ret) {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "pthread join failed, ret = " << ret
                                                  << " name = " << m_name;
                throw std::logic_error("pthread join failed");
            }
            m_thread = 0;
        }
    }

    Thread::~Thread() {
        if (m_thread) {
            pthread_detach(m_thread);
        }
    }

    void Thread::setName(const std::string &name) {
        if (t_thread) {
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }

}
