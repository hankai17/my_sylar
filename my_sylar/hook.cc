#include "hook.hh"
#include "log.hh"
#include "my_sylar/config.hh"
#include "fiber.hh"
#include "iomanager.hh"
#include "fd_manager.hh"
#include "macro.hh"

#include <dlfcn.h>
#include <iostream>

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

namespace sylar {
    sylar::ConfigVar<uint64_t>::ptr g_tcp_connection_timeout =
            sylar::Config::Lookup<uint64_t>("tcp.connection.timeout", 5000, "tcp connection timeout");
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
XX(closesocket);
XX(shutdown);
XX(fcntl);
XX(ioctl);
XX(getsockopt);
XX(setsockopt);
#undef XX
    }

    static uint64_t s_connect_timeout = -1;
    struct _HookIniter {
        _HookIniter() {
            hook_init();
            s_connect_timeout = g_tcp_connection_timeout->getValue();
            g_tcp_connection_timeout->addListener("tcp.connection.timeout",[](const int& old_value, const int& new_value){
                SYLAR_LOG_DEBUG(g_logger) << "tcp connection timeout change from "
                << old_value << " to " << new_value;
                s_connect_timeout = new_value; // ???
            });
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
    ssize_t n = fun(fd, std::forward<Args>(args)...); // https://www.cnblogs.com/kex1n/p/7662036.html
    while (n == -1 && errno == EINTR) {
        n = fun(fd, std::forward<Args>(args)...);
    }

    if (n == -1 && errno == EAGAIN) { // 这个函数即是一次io操作 只有读到最后没有数据时才反-1
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        if (to != (uint64_t)-1) { // 即上层设置了此次io的超时时间
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event]() { // 假设从上树发生io的几行的用时时间为0
                auto t = winfo.lock(); // io结束 & 超时 不确定哪个先执行 这就是weakptr的应用时机
                if (!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT; // 110
                iom->cancelEvent(fd, (sylar::IOManager::Event)(event));
            }, winfo);
        }

        int ret = iom->addEvent(fd, (sylar::IOManager::Event)(event));
        if (SYLAR_UNLICKLY(ret)) {
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
            SYLAR_ASSERT(sylar::Fiber::GetThis()->getState() == sylar::Fiber::EXEC);
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
XX(closesocket) \
XX(shutdown) \
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
    SYLAR_LOG_ERROR(g_logger) << "hook: socket() return fd: " << fd;
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
    std::weak_ptr<timer_info> winfo(tinfo); // owner持有shared_ptr child持有weak_ptr

    if (timeout_ms != (uint64_t)-1) {
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

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, sylar::s_connect_timeout);
}


int accept(int s, struct sockaddr* addr, socklen_t* addrlen) {
    int fd = do_io(s, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if (fd > 0) {
        sylar::FdManager::getFdMgr()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void* buf, size_t count) {
    return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}


ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void* buf, size_t count) {
    return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void* msg, size_t len, int flags) {
    return do_io(s, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void* msg, size_t len, int flags, const struct sockaddr* to, socklen_t tolen) {
    return do_io(s, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
    if (!sylar::t_hook_enable) {
        return close_f(fd);
    }
    sylar::FdCtx::ptr ctx = sylar::FdManager::getFdMgr()->get(fd);
    if (ctx) {
        auto iom = sylar::IOManager::GetThis();
        if (iom) {
            bool ret = iom->cancelAll(fd);
            SYLAR_LOG_DEBUG(g_logger) << "hook close fd: " << fd << " ret: " << ret;
        }
        sylar::FdManager::getFdMgr()->del(fd);
    }
    return close_f(fd); // 引用计数仍大于0时，这个close调用就不会引发TCP的四路握手断连过程
}

int closesocket(int fd) {
    return close(fd);
}

int shutdown(int fd, int how) {
    if (!sylar::t_hook_enable) {
        return shutdown_f(fd, how);
    }
    sylar::FdCtx::ptr ctx = sylar::FdManager::getFdMgr()->get(fd);
    if (ctx) {
        auto iom = sylar::IOManager::GetThis();
        if (iom) {
            sylar::IOManager::Event event = sylar::IOManager::Event::NONE;
            switch (how) {
                case 0: {
                    event = sylar::IOManager::Event::READ;
                    break;
                }
                case 1: {
                    event = sylar::IOManager::Event::WRITE;
                    break;
                }
                default : {
                    break;
                }
            }
            if (event == sylar::IOManager::Event::NONE) {
                bool ret = iom->cancelAll(fd);
                SYLAR_LOG_DEBUG(g_logger) << "hook shutdown fd: " << fd << " ret: " << ret;
            } else {
                bool ret = iom->cancelEvent(fd, event);
                SYLAR_LOG_DEBUG(g_logger) << "hook shutdown fd: " << fd << " ret: " << ret;
            }
        }
        //sylar::FdManager::getFdMgr()->del(fd);
    }
    return shutdown_f(fd, how); // 引用计数仍大于0时，这个close调用就不会引发TCP的四路握手断连过程
}

int fcntl(int fd, int cmd, ... /* arg */ ) {
    va_list va;
    va_start(va, cmd);
    switch (cmd) {
        case F_SETFL: {
            int arg = va_arg(va, int);
            va_end(va);
            sylar::FdCtx::ptr ctx = sylar::FdManager::getFdMgr()->get(fd);
            if (!ctx || ctx->isClosed() || !ctx->isSocket()) {
                return fcntl_f(fd, cmd, arg);
            }
            ctx->setUserNonblock(arg & O_NONBLOCK);
            if (ctx->getSysNonblock()) {
                arg |= O_NONBLOCK;
            } else {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        }
        break;
        case F_GETFL: {
            va_end(va);
            int arg = fcntl_f(fd, cmd);
            sylar::FdCtx::ptr ctx = sylar::FdManager::getFdMgr()->get(fd);
            if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
                return arg;
            }
            if(ctx->getUserNonblock()) {
                return arg | O_NONBLOCK;
            } else {
                return arg & ~O_NONBLOCK;
            }
        }
        break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
        {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
        {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
        {
            struct flock* arg = va_arg(va, struct flock*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
        {
            struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request) {
        bool user_nonblock = !!*(int*)arg;
        sylar::FdCtx::ptr ctx = sylar::FdManager::getFdMgr()->get(d);
        if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if(!sylar::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET) {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            sylar::FdCtx::ptr ctx = sylar::FdManager::getFdMgr()->get(sockfd);
            if(ctx) {
                const timeval* v = (const timeval*)optval;
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}

