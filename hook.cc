#include "hook.hh"
#include "log.hh"
#include "fiber.hh"
#include "iomanager.hh"

#include <dlfcn.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static thread_local bool t_hook_enable = false;

    bool is_hook_enable() {
        return t_hook_enable;
    }

    void set_hook_enable(bool flag) {
        t_hook_enable = flag;
    }

    void hook_init() {
        static bool is_inited = false;
        if (is_inited) {
            return;;
        }
            // define xx(sleep) sleep_f = sleep_fun(dlsym)(RTLD_NEXT, sleep) //即把系统sleep更名为sleep_f
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name)
XX(sleep);
XX(usleep);
XX(nanosleep);
XX(socket);
XX(connect);
XX(accept);
XX(read);
XX(readv);
XX(recv);
XX(recvfrom);
XX(recvmsg);
XX(write);
XX(writev);
XX(send);
XX(sendto);
XX(sendmsg);
XX(close);
XX(fcntl);
XX(ioctl);
XX(getsockopt);
#undef XX
    }

    struct _HookIniter {
        _HookIniter() {
            hook_init();
        }
    };
    static _HookIniter s_hook_initer;
}

extern "C" {
// define xx(sleep) sleep_fun sleep_f = nullptr
#define XX(name) name ## _fun name ## _f = nullptr;
XX(sleep) \
XX(usleep) \
XX(nanosleep)
XX(socket) \
XX(connect) \
XX(accept) \
XX(read) \
XX(readv) \
XX(recv) \
XX(recvfrom) \
XX(recvmsg) \
XX(write) \
XX(writev) \
XX(send) \
XX(sendto) \
XX(sendmsg) \
XX(close) \
XX(fcntl) \
XX(ioctl) \
XX(getsockopt) \
XX(setsockopt)
#undef XX

unsigned int sleep(unsigned int seconds) {
    if (!sylar::t_hook_enable) {
        return sleep_f(seconds);
    }

    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(seconds * 1000, [fiber, iom]() {
        iom->schedule(fiber);
    });
    sylar::Fiber::YeildToHold();
    return 0;
}

int usleep(useconds_t usec) {
    if (!sylar::t_hook_enable) {
        return usleep_f(usec);
    }

    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(usec / 1000, [fiber, iom]() {
        iom->schedule(fiber);
    });
    sylar::Fiber::YeildToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if(!sylar::t_hook_enable) {
        return nanosleep_f(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 /1000;
    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(timeout_ms, std::bind((void(sylar::Scheduler::*)
                                                (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
            ,iom, fiber, -1));
    sylar::Fiber::YeildToHold();
    return 0;
}

}

