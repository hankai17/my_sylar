#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__

#include <memory>
#include <string>
#include <list>
#include <vector>

#include "thread.hh"
#include "fiber.hh"

namespace sylar {
    class Scheduler {
    public:
        typedef std::shared_ptr<Scheduler> ptr;
        typedef Mutex MutexType;

        Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");
        virtual ~Scheduler();

        const std::string& getName() const { return m_name; }
        static Fiber* GetMainFiber();

        static void SetMainFiber(Fiber *fiber);
        static Scheduler* GetThis();

        void start();
        void stop();
        bool hasIdleFiber() const { return m_activeIdleFiberCount > 0; }

        template <typename T>
        void schedule(T t, int thread = -1) {
            bool need_tickle = false;
            {
                MutexType::Lock lock(m_mutex);
                need_tickle = scheduleNoLock(t, thread);
            }

            if (need_tickle) { // If m_fibers is empty we should tickle
                tickle(); // Why tickle ? Because If there are cbs in list We should NOT sleep in epoll
            }
        }

        template <typename T>
        void schedule(T begin, T end, int thread = -1) {
            bool need_tickle = false;
            {
                MutexType::Lock lock(m_mutex);
                while (begin != end) {
                    need_tickle = scheduleNoLock(&*begin, thread) || need_tickle;
                    ++begin;
                }
            }

            if (need_tickle) {
                tickle();
            }
        }
        void switchTo(int thread = -1);
        std::ostream& dump(std::ostream& os);

    protected:
        virtual void tickle();
        void run();
        virtual bool stopping();
        virtual void idle();
        void setThis();

    private:
        struct FiberAndThread {
            Fiber::ptr              fiber;
            std::function<void()>   cb;
            int                     thread;

            FiberAndThread(Fiber::ptr f, int thr)
                    :fiber(f),
                     thread(thr) {
            }

            FiberAndThread(Fiber::ptr* f, int thr) // pointer && swap
                    :thread(thr) {
                fiber.swap(*f);
            }

            FiberAndThread(std::function<void()> f, int thr)
                    :cb(f),
                     thread(thr) {
            }

            FiberAndThread(std::function<void()>* f, int thr)
                    : thread(thr) {
                cb.swap(*f);
            }

            FiberAndThread()
                    :thread(-1) {
            }

            void reset() {
                fiber = nullptr;
                cb = nullptr;
                thread = -1;
            }
        };

        template <typename T>
        bool scheduleNoLock(T t, int thread) {
            bool need_tickle = m_fibers.empty();
            FiberAndThread ft(t, thread);
            if (ft.fiber || ft.cb) {
                m_fibers.push_back(ft);
            }
            return need_tickle;
        }

        MutexType                   m_mutex;
        std::string                 m_name;
        std::list<FiberAndThread>   m_fibers; // Gobal 3P list
        std::vector<Thread::ptr>    m_threads;
        Fiber::ptr                  m_rootFiber;

    protected: // Why use protected
        size_t                      m_threadCounts = 0;
        std::vector<int>            m_threadIds {};
        int                         m_rootThreadId = 0;
        bool                        m_stopping = false;
        std::atomic<size_t>         m_activeFiberCount = {0}; // Not contain m_activeIdleFiber
        std::atomic<size_t>         m_activeIdleFiberCount = {0};
        bool                        m_autostop = false; // For system test
    };

    class SchedulerSwitcher : public Nocopyable {
    public:
        SchedulerSwitcher(Scheduler* target = nullptr);
        ~SchedulerSwitcher();
    private:
        Scheduler*  m_caller;
    };
}

#endif
