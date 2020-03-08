#ifndef __WORKER_HH__
#define __WORKER_HH__

#include "nocopy.hh"
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include "thread.hh"
#include "scheduler.hh"
#include "iomanager.hh"
#include "log.hh"

namespace sylar {
    class Worker : private Nocopyable, public std::enable_shared_from_this<Worker> {
    public:
        typedef std::shared_ptr<Worker> ptr;
        static Worker::ptr Create(uint32_t bath_size, sylar::Scheduler* s = sylar::Scheduler::GetThis()) {
            return std::make_shared<Worker>(bath_size, s);
        }
        Worker(uint32_t bath_size, sylar::Scheduler* s = sylar::Scheduler::GetThis());
        ~Worker();
        void schedule(std::function<void()> cb, int thread = -1);
        void waitAll();

    private:
        void doWork(std::function<void()> cb);

    private:
        uint32_t        m_batchSize;
        bool            m_finish;
        Scheduler*      m_scheduler;
        FiberSemaphore  m_sem;
    };

    class WorkerManager {
    public:
        WorkerManager();
        void add(Scheduler::ptr s);
        Scheduler::ptr get(const std::string& name);
        IOManager::ptr getAsIOManager(const std::string& name);

        template <typename FiberOrCb>
        void schedule(const std::string& name, FiberOrCb fc, int thread = -1) {
            auto s = get(name);
            if (s) {
                s->schedule(fc, thread);
            } else {
                static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
                SYLAR_LOG_ERROR(g_logger) << "schedule name: " << name
                << " not exists";
            }
        }

        template <typename Iter>
        void schedule(const std::string& name, Iter begin, Iter end) {
            auto s = get(name);
            if (s) {
                s->schedule(begin, end);
            } else {
                static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
                SYLAR_LOG_ERROR(g_logger) << "schedule name: " << name
                                          << " not exists";
            }
        }

        bool init();
        void stop();
        bool isStoped() const { return m_stop; }
        std::ostream& dump(std::ostream& os);
        uint32_t getCount();

    private:
        bool        m_stop;
        std::map<std::string, std::vector<Scheduler::ptr> > m_datas;

    };

}

#endif
