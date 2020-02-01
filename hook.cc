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
XX(sleep)
XX(usleep)
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

}

