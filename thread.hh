#ifndef __THREAD_HH__
#define __THREAD_HH__

#include <memory>
#include <thread>
#include <functional>
#include <thread.h>

namespace sylar {
    class Thread {
    public:
        typedef std::shared_ptr<Thread> ptr;
        Thread(std::function<void()> cb, const std::string& name);
        ~Thread();

        pid_t getId() const { return m_id; }
        const std::string& getName() const { return m_name; }
        void join();

    private:
        pid_t                   m_id = -1;
        std::function<void()>   m_cb;
        std::string             m_name;
        pthread_t               m_thread = 0;
    };
}

#endif