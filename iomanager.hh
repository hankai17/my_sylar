#ifndef _IOMANAGER_HH__
#define _IOMANAGER_HH__

#include "scheduler.hh"
#include <functional>

namespace sylar {
    class IOManager : public Scheduler {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWMutexType;

        enum Event {
            NONE    = 0x0,
            READ    = 0x1, // EPOLLIN
            //WRITE   = 0x2, // EPOLLOUT // Define in epoll.h. There diff from muduo
            WRITE   = 0x4, // EPOLLOUT
        };

    private:
        struct FdContext {
            typedef Mutex MutexType;
            struct EventContext {
                Scheduler* scheduler = nullptr;
                Fiber::ptr fiber;
                std::function<void()> cb;
            };

            EventContext& getContext(Event event);
            void resetContext(EventContext& ctx); // not const
            void triggerEvent(Event event);

            EventContext read;
            EventContext write;
            int fd = 0;
            Event events = NONE;
            MutexType mutex;
        };

    public:
        IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");
        ~IOManager();

        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
        bool delEvent(int fd, Event event);
        bool cancelEvent(int fd, Event event);
        bool cancelAll(int fd);

        static IOManager* GetThis();

    protected:
        void tickle() override;
        bool stopping() override;
        void idle() override;

        void contextResize(size_t size);

    private:
        int m_epfd = 0;
        int m_tickleFds[2];
        int m_wakeupFd = -1;
        std::atomic<size_t> m_pendingEventCount = {0};
        RWMutexType m_mutex;
        std::vector<FdContext*> m_fdContexts;
    };

}

#endif
