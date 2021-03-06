#ifndef _IOMANAGER_HH__
#define _IOMANAGER_HH__

#include "scheduler.hh"
#include "timer.hh"
#include <functional>

namespace sylar {
    class IOManager : public Scheduler, public TimerManager {
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
            bool triggerEvent(Event event);

            EventContext read;
            EventContext write;
            int fd = 0;
            Event events = NONE;
            MutexType mutex;
        };

    public:
        IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "", bool need_start = false);
        ~IOManager();

        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
        bool delEvent(int fd, Event event);
        bool cancelEvent(int fd, Event event);
        bool cancelAll(int fd);
        void fake_start();

        static IOManager* GetThis();
        bool stopping(uint64_t& timeout);

    protected:
        void tickle() override;
        bool stopping() override;
        void idle() override;
        void onTimerInsertedAtFront() override;

        void contextResize(size_t size);

    private:
        bool        m_needstart;
        int         m_epfd = 0;
        int         m_tickleFds[2];
        int         m_wakeupFd = -1;
        RWMutexType m_mutex;
        std::atomic<size_t> m_pendingEventCount = {0};
        std::vector<FdContext*> m_fdContexts;
    };

}

#endif
