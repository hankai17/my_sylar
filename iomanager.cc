#include "iomanager.hh"
#include "macro.hh"

#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    void IOManager::contextResize(size_t size) {
        m_fdContexts.resize(size);
        for (size_t i = 0; i < m_fdContexts.size(); i++) {
            if (!m_fdContexts[i]) {
                m_fdContexts[i] = new FdContext;
                m_fdContexts[i]->fd = i;
            }
        }
    }

    IOManager::FdContext::EventContext& IOManager::FdContext::getContext(sylar::IOManager::Event event) {
        switch (event) {
            case IOManager::READ:
                return read;
            case IOManager::WRITE:
                return write;
            default:
                SYLAR_ASSERT(false);
        }
    }

    IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
    :Scheduler(threads, use_caller, name) {
        m_epfd = epoll_create(5000);
        SYLAR_ASSERT(m_epfd);

        int ret = pipe(m_tickleFds);
        SYLAR_ASSERT(!ret)

        epoll_event event;
        memset(&event, 0, sizeof(epoll_event));
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = m_tickleFds[0];

        ret = fcntl(m_tickleFds[0], F_SETFL, 0, O_NONBLOCK);
        SYLAR_ASSERT(!ret)

        ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
        SYLAR_ASSERT(!ret)

        contextResize(32);

        start();
    }

    int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
        FdContext* fd_ctx = nullptr;
        RWMutexType::ReadLock lock(m_mutex);
        if ((int)m_fdContexts.size() > fd) {
            fd_ctx = m_fdContexts[fd];
            lock.unlock();
        } else {
            lock.unlock();
            RWMutexType::WriteLock lock1(m_mutex);
            contextResize(fd * 1.5);
            fd_ctx = m_fdContexts[fd];
        }

        FdContext::MutexType::Lock lock2(fd_ctx->mutex);
        if (fd_ctx->events & event) {
            SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd = " << fd
            << " event = " << fd_ctx->events;
            SYLAR_ASSERT(!(fd_ctx->events & event));
        }

        int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        epoll_event epevent;
        epevent.events = EPOLLET | fd_ctx->events | event;
        epevent.data.ptr = fd_ctx;

        int ret = epoll_ctl(m_epfd, op, fd, &epevent);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << ", " << fd << ", " << epevent.events << "):"
            << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return -1; // TODO
        }

        ++m_pendingEventCount;
        fd_ctx->events = (Event)(fd_ctx->events | event);
        FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
        SYLAR_ASSERT(!event_ctx.scheduler
        && !event_ctx.fiber
        && !event_ctx.cb);
        event_ctx.scheduler = Scheduler::GetThis();

        if (cb) {
            //event_ctx.cb = cb;
            event_ctx.cb.swap(cb);
        } else {
            event_ctx.fiber = Fiber::GetThis();
            SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
        }
        return 0;
    }

    bool IOManager::delEvent(int fd, Event event) {
        return false;
    }

    bool IOManager::cancelEvent(int fd, Event event) {
        return false;
    }

    bool IOManager::cancelAll(int fd) {
        return false;
    }

    IOManager* IOManager::GetThis() {
        return nullptr;
    }

    void IOManager::tickle() {

    }

    bool IOManager::stopping() {
        return false;
    }

    void IOManager::idle() {

    }

    IOManager::~IOManager() {

    }

}

