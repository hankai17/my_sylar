#include "thread.hh"
#include "log.hh"

namespace sylar {
    Thead::Thread(std::function<void()> cb, std::string& name)
    : m_cb(cb),
    m_name(name) {
        if (name.empty()) {
            m_name = "UNKNOW";
        }
        int ret = pthread_create(&m_thread, nullptr, &Thread::run, this);
        if (ret) {
            SYLAR_LOG_ERROR()
        }
    }

    ~Thread::Thread() {
       if ()
    }

}