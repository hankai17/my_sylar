#include "iomanager.hh"
#include "macro.hh"

#include <sys/epoll.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    IOManager* IOManager::GetThis() {
        return dynamic_cast<IOManager*>(Scheduler::GetThis());
    }

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

    void IOManager::FdContext::resetContext(EventContext& ctx) {
        ctx.scheduler = nullptr;
        ctx.fiber.reset();
        ctx.cb = nullptr;
    }

    void IOManager::FdContext::triggerEvent(IOManager::Event event) {
      SYLAR_ASSERT(events & event);
      events = (Event)(events & ~event); // ?
      EventContext& ctx = getContext(event);
      if (ctx.cb) {
          ctx.scheduler->schedule(&ctx.cb);
      } else {
          ctx.scheduler->schedule(&ctx.fiber);
      }
      ctx.scheduler = nullptr;
      return;
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

    IOManager::~IOManager() {

    }

    // 不能添加重复事件 即已经监控了读还要监听读是不对的
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

    // 不能删除不存在的事件
    // 不能删除空事件 只能删除读 写事件
    bool IOManager::delEvent(int fd, Event event) {
        RWMutexType::ReadLock lock(m_mutex);
        if (fd >= (int)m_fdContexts.size()) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();

        FdContext::MutexType::Lock lock1(fd_ctx->mutex);
        if (!(fd_ctx->events & event)) {
            return false;
        }

        Event new_events = (Event)(fd_ctx->events & ~event);
        int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int ret = epoll_ctl(m_epfd, op, fd, &epevent);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << ", " << fd << ", " << epevent.events << "):"
            << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false; // TODO
        }

        --m_pendingEventCount;
        fd_ctx->events = new_events;
        FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
        fd_ctx->resetContext(event_ctx);
        return true;
    }

    bool IOManager::cancelEvent(int fd, Event event) {
        RWMutexType::ReadLock lock(m_mutex);
        if (fd >= (int)m_fdContexts.size()) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();

        FdContext::MutexType::Lock lock1(fd_ctx->mutex);
        if (!(fd_ctx->events & event)) {
            return false;
        }

        Event new_events = (Event)(fd_ctx->events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int ret = epoll_ctl(m_epfd, op, fd, &epevent);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << ", " << fd << ", " << epevent.events << "):"
            << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false; // TODO
        }
        fd_ctx->triggerEvent(event); // Why trigger event?
        --m_pendingEventCount;
        return true;
    }

    bool IOManager::cancelAll(int fd) {
        RWMutexType::ReadLock lock(m_mutex);
        if (fd >= (int)m_fdContexts.size()) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();

        FdContext::MutexType::Lock lock1(fd_ctx->mutex);
        if (!fd_ctx->events) {
            return false;
        }

        int op = EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = 0;
        epevent.data.ptr = fd_ctx;

        int ret = epoll_ctl(m_epfd, op, fd, &epevent);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << ", " << fd << ", " << epevent.events << "):"
            << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false; // TODO
        }

        if (fd_ctx->events & READ) {
            fd_ctx->triggerEvent(READ);
        }
        if (fd_ctx->events & WRITE) {
            fd_ctx->triggerEvent(WRITE);
        }
        --m_pendingEventCount;
        SYLAR_ASSERT(fd_ctx->events == 0);
        return true;
    }

    void IOManager::tickle() {
        //if (hasIdleThread) {
        //}
        std::cout<<"tickle......"<<std::endl;
        int ret = write(m_tickleFds[1], "M", 1);
        SYLAR_ASSERT(ret == 1);
    }

    bool IOManager::stopping() {
        return Scheduler::stopping()
          && m_pendingEventCount == 0;
    }

    void IOManager::idle() {
        epoll_event* events = new epoll_event[64](); // ?
        std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr) {
              delete[] ptr;
              });
        for (;;) {
            /*  I DO NOT UNDERSTAND stop & stopping logic !
            if (stopping()) {
                SYLAR_LOG_INFO(g_logger) << "name=" << getName() << " idle stopping exit";
                break;
            }
             */

            int ret = 0;
            do {
                static const int MAX_TIMEOUT = 5000;
                ret = epoll_wait(m_epfd, events, 4, MAX_TIMEOUT);
                if (ret < 0 && errno == EINTR) {
                } else {
                    break;
                }
            } while(1);

            for (int i = 0; i < ret; i++) {
                std::cout<<"i: "<<i<<std::endl;
                epoll_event& event = events[i];
                if (event.data.fd == m_tickleFds[0]) {
                    uint8_t goddess;
                    int ret = 0;
                    std::cout<<"before read m_tickleFds[0]" <<std::endl;
                    while ((ret = read(m_tickleFds[0], &goddess, 1)) > 0)
                    {
                       std::cout << "!!!read ret: "<<ret <<std::endl;
                    };
                    //read(m_tickleFds[0], &goddess, 1);
                    continue;
                }

                FdContext* fd_ctx = (FdContext*)event.data.ptr;
                FdContext::MutexType::Lock lock(fd_ctx->mutex);
                if (event.events & (EPOLLERR | EPOLLHUP)) {
                    event.events |= EPOLLIN | EPOLLOUT;
                }
                int real_events = NONE; // Only read or write
                if (event.events & EPOLLIN) {
                    real_events |= READ;
                }
                if (event.events & EPOLLOUT) {
                    real_events |= WRITE;
                }
                if ((fd_ctx->events & real_events) == NONE) {
                    continue;
                }

                int left_event = (fd_ctx->events & ~real_events);
                int op = left_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
                event.events = EPOLLET | left_event;

                int ret = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
                if (ret) {
                    SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                    << op << ", " << fd_ctx->fd << ", " << event.events << "):"
                    << ret << " (" << errno << ") (" << strerror(errno) << ")";
                    continue;
                }

                if (real_events & READ) {
                    fd_ctx->triggerEvent(READ);
                }
                if (real_events & WRITE) {
                    fd_ctx->triggerEvent(WRITE);
                }
                --m_pendingEventCount;

            }

            if (0) {
                struct timeval l_now = {0};
                gettimeofday(&l_now, NULL);
                long end_time = ((long) l_now.tv_sec) * 1000 + (long) l_now.tv_usec / 1000;
                std::cout << "idle...  " << end_time << std::endl;
            }

            Fiber::ptr cur = Fiber::GetThis();
            //cur->setState(sylar::Fiber::HOLD); // It is so ugly
            auto raw_ptr = cur.get();
            cur.reset();
            raw_ptr->swapOut();
        }

    }

}

