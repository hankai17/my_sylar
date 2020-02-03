#include "hook.hh"
#include "log.hh"
#include "fiber.hh"
#include "iomanager.hh"
#include "fd_manager.hh"

#include <dlfcn.h>

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

namespace sylar {
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

struct timer_info {
    int cancelled = 0;
};

template <typename OriginFunc, typename... Args>
static ssize_t do_io(int fd, OriginFunc fun, const char* hook_fun_name,
                     uint32_t event, int timeout_so, Args&&... args) {
    if (!sylar::t_hook_enable) {
        return fun(fd, std::forward<Args>(args)...);
    }

    sylar::FdCtx::ptr ctx = sylar::FdManager::getFdMgr()->get(fd);
    if (!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    if (ctx->isClosed()) {
        errno = EBADE;
        return -1;
    }

    if (!ctx->isSocket() || ctx->getUserNonblock()) {
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

    retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR) {
        n = fun(fd, std::forward<Args>(args)...);
    }

    if (n == -1 && errno == EAGAIN) {
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        if (to != -1) { // 即上层设置超时时间了
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event]() { // 假设从上树发生io的几行的用时时间为0
                auto t = winfo.lock();
                if (!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT; // 110
                iom->cancelEvent(fd, (sylar::IOManager::Event)(event));
            }, winfo);
        }

        int ret = iom->addEvent(fd, (sylar::IOManager::Event)(event));
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << hook_fun_name << " addEvent("
            << fd << " , " << event << ")";
            if (timer) {
                timer->cancel();
            }
        } else { // success add
            sylar::Fiber::YeildToHold();
            if (timer) {
                timer->cancel();
            }
            if (tinfo->cancelled) {
                errno = tinfo->cancelled;
                return -1;
            }
            goto retry;
        }
    }
    return n;
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

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(timeout_ms, std::bind((void(sylar::Scheduler::*)
                                                (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
            ,iom, fiber, -1));
    sylar::Fiber::YeildToHold();
    return 0;
}


int socket(int domain, int type, int protocol) {
    if (!sylar::t_hook_enable) {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if (fd == -1) {
        return fd;
    }
    sylar::FdManager::getFdMgr()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) {
    if (!sylar::t_hook_enable) {
        return connect_f(fd, addr, addrlen);
    }
    sylar::FdCtx::ptr ctx = sylar::FdManager::getFdMgr()->get(fd);
    if (!ctx || ctx->isClosed()) {
        errno = EBADF;
        return -1;
    }

    if (!ctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    if (ctx->getUserNonblock()) {
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    if (n == 0) {
        return 0;
    } else if (n != -1 || errno != EINPROGRESS) {
        return n;
    }

    sylar::IOManager* iom = sylar::IOManager::GetThis();
    sylar::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    if (timeout_ms != -1) {
        timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom]() {
            auto t = winfo.lock();
            if (!t || t->cancelled) {
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, sylar::IOManager::WRITE);
        }, winfo);
    }

    int ret = iom->addEvent(fd, sylar::IOManager::WRITE);
    if (ret) {
        if (timer)  {
            timer->cancel();
        }
        SYLAR_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    } else {
        sylar::Fiber::YeildToHold();
        if (timer) {
            timer->cancel();
        }

        if (tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }
    }

    int error = -1;
    socklen_t len = sizeof(int);
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    if (!error) {
        return 0; // success
    } else {
        errno = error;
        return -1;
    }
}

}

