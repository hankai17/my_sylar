#ifndef __THREAD_HH__
#define __THREAD_HH__

#include <memory>
#include <string>
#include <thread>
#include <functional>
#include <pthread.h>

namespace sylar {

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
    };
}

#endif
