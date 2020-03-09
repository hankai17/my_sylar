#include "worker.hh"
#include "config.hh"
#include "util.hh"

#include <map>
#include <yaml-cpp/yaml.h>
#include <string>

namespace sylar {
    static sylar::ConfigVar<std::map<std::string, std::map<std::string, std::string> > >::ptr g_worker_config =
            sylar::Config::Lookup("workers", std::map<std::string, std::map<std::string, std::string> >(), "worker config");

    /*
    template <>
    class Lexicalcast<std::string, std::map<std::string, std::map<std::string, std::string> > > {
    };
     */
    // This is base type so dont rewrite it again

    struct WorkerConfigIniter {
        WorkerConfigIniter() {
            g_worker_config->addListener("workers", [](const std::map<std::string, std::map<std::string, std::string> >& old_val,
                    const std::map<std::string, std::map<std::string, std::string> >& new_val) {
                const_cast<
                    std::map<std::string, std::map<std::string, std::string> >&
                    > (old_val) = new_val;
            });
        }
    };
    static WorkerConfigIniter __worker_config_init;

    Worker::Worker(uint32_t bath_size, sylar::Scheduler* s)
    : m_batchSize(bath_size),
    m_finish(false),
    m_scheduler(s) {
    }

    Worker::~Worker() {
        waitAll();
    }

    void Worker::schedule(std::function<void()> cb, int thread) {
        m_sem.wait();
        m_scheduler->schedule(std::bind(&Worker::doWork, shared_from_this(), cb), thread);
    }

    void Worker::waitAll() {
        if (!m_finish) {
            m_finish = true;
            for (uint32_t i = 0; i < m_batchSize; i++) {
                m_sem.wait();
            }
        }
    }

    void Worker::doWork(std::function<void()> cb) {
        cb();
        m_sem.notify();
    }

    WorkerManager* WorkerManager::m_workermgr(new WorkerManager);

    WorkerManager::WorkerManager()
    : m_stop(false) {
    }

    void WorkerManager::add(Scheduler::ptr s) {
        m_datas[s->getName()].push_back(s);
    }

    Scheduler::ptr WorkerManager::get(const std::string& name) {
        auto it = m_datas.find(name);
        if (it == m_datas.end()) {
            return nullptr;
        }
        if (it->second.size() == 1) {
            return it->second[0];
        }
        return it->second[rand() % it->second.size()];
    }

    IOManager::ptr WorkerManager::getAsIOManager(const std::string& name) {
        return std::dynamic_pointer_cast<IOManager>(get(name));
    }
    // workers:
    //    - io:
    //      thread_num: 4

    bool WorkerManager::init() {
        auto workers = g_worker_config->getValue();
        for (const auto& i : workers) {
            std::string name = i.first;
            int32_t thread_num = sylar::GetParaValue(i.second, "thread_num", 1);
            int32_t worker_num = sylar::GetParaValue(i.second, "worker_num", 1);
            for (int32_t x = 0; x < worker_num; x++) {
                Scheduler::ptr s;
                if (!x) {
                    s = std::make_shared<IOManager>(thread_num, false, name);
                } else {
                    s = std::make_shared<IOManager>(thread_num, false, name + "-" + std::to_string(x));
                }
                add(s);
            }
        }
        m_stop = m_datas.empty();
        return true;
    }

    void WorkerManager::stop() {
        if (m_stop) {
            return;
        }
        for (const auto& i : m_datas) {
            for (const auto& j : i.second) {
                j->schedule([](){});
                j->stop();
            }
        }
        m_datas.clear();
        m_stop = true;
    }

    std::ostream& WorkerManager::dump(std::ostream& os) {
        for (const auto& i : m_datas) {
            for (const auto& j : i.second) {
                j->dump(os) << std::endl;
            }
        }
        return os;
    }

    uint32_t WorkerManager::getCount() {
        return m_datas.size();
    }

}