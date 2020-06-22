#include "my_sylar/db/fox_thread.hh"
#include "my_sylar/log.hh"
#include "my_sylar/util.hh"

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    thread_local FoxThread* s_thread = nullptr;
    static std::map<uint64_t, std::string> s_thread_names;
    static RWMutex s_thread_mutex;

    FoxThread* FoxThread::GetThis() {
      return s_thread;
    }

    void FoxThread::setThis() {
        m_name = m_name + "_" + std::to_string(sylar::GetThreadId());
        s_thread = this;

        RWMutex::WriteLock lock(s_thread_mutex);
        s_thread_names[sylar::GetThreadId()] = m_name;
    }

    void FoxThread::unsetThis() {
        s_thread = nullptr;
        RWMutex::WriteLock lock(s_thread_mutex);
        s_thread_names.erase(sylar::GetThreadId());
    }

    void FoxThread::read_cb(evutil_socket_t sock, short which, void* arg) { // 读回调 回调所有m_callbacks
        FoxThread* thread = static_cast<FoxThread*>(arg);
        uint8_t cmd[4096];
        if (recv(sock, cmd, sizeof(cmd), 0) > 0) {
            std::list<callback> callbacks;
            RWMutex::WriteLock lock(thread->m_mutex); // ? private ??? // static成员函数使用 对象的非static成员 而且是私有的
            callbacks.swap(thread->m_callbacks);
            lock.unlock();

            thread->m_working = true;
            for (auto it = callbacks.begin(); it != callbacks.end(); ++it) {
                if (*it) {
                    try {
                        (*it)();
                    } catch (std::exception& ex) {
                        SYLAR_LOG_ERROR(g_logger) << " exception: " << ex.what();
                    } catch (const char* c) {
                        SYLAR_LOG_ERROR(g_logger) << " exception: " << c;
                    } catch (...) {
                        SYLAR_LOG_ERROR(g_logger) << "uncatch exception";
                    }
                } else {
                    event_base_loopbreak(thread->m_base);
                    thread->m_start = false;
                    thread->unsetThis();
                    break;
                }
            }
            sylar::Atomic::addFetch(thread->m_total, callbacks.size());
            thread->m_working = false;
        }
    }

    FoxThread::FoxThread(const std::string& name, struct event_base* base): // 构造fds 一旦有事件到来则调read_cb
      m_read(0),
      m_write(0),
      m_base(NULL),
      m_event(NULL),
      m_thread(NULL),
      m_name(name),
      m_working(false),
      m_start(false),
      m_total(0) {
          int fds[2];
          if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
              throw std::logic_error("thread init error");
          }

          evutil_make_socket_nonblocking(fds[0]);
          evutil_make_socket_nonblocking(fds[1]);

          m_read = fds[0];
          m_write = fds[1];

          if (base) {
              m_base = base;
              setThis();
          } else {
              m_base = event_base_new();
          }
          m_event = event_new(m_base, m_read, EV_READ | EV_PERSIST, read_cb, this);
          event_add(m_event, NULL);
    }

    void FoxThread::dump(std::ostream& os) {
        RWMutex::ReadLock lock(m_mutex);
        os << "thread name: " << m_name
          << " working: " << m_working
          << " tasks: " << m_callbacks.size()
          << " total: " << m_total
          << std::endl;
    }

    std::thread::id FoxThread::getId() const {
        if (m_thread) {
            return m_thread->get_id();
        }
        return std::thread::id();
    }

    void* FoxThread::getData(const std::string& name) {
        auto it = m_datas.find(name);
        return it == m_datas.end() ? nullptr : it->second;
    }

    void FoxThread::start() {
        if (m_thread) {
            throw std::logic_error("FoxThread is running");
        }
        m_thread = new std::thread(std::bind(&FoxThread::thread_cb, this));
        m_start = true;
    }

    void FoxThread::thread_cb() {
        setThis();
        pthread_setname_np(pthread_self(), m_name.substr(0, 15).c_str());
        if (m_initCb) {
            m_initCb(this);
            m_initCb = nullptr;
        }
        event_base_loop(m_base, 0);
    }

    bool FoxThread::dispatch(callback cb) {
        RWMutex::WriteLock lock(m_mutex);
        m_callbacks.push_back(cb);

        lock.unlock();
        uint8_t cmd = 1;
        if (send(m_write, &cmd, sizeof(cmd), 0) < 0) {
            return false;
        }
        return true;
    }

    bool FoxThread::dispatch(uint32_t id, callback cb) {
        return dispatch(cb);
    }

    bool FoxThread::batchDispatch(const std::vector<callback>& cbs) {
        RWMutex::WriteLock lock(m_mutex);
        for (const auto& i : cbs) {
            m_callbacks.push_back(i);
        }
        lock.unlock();
        uint8_t cmd = 1;
        if (send(m_write, &cmd, sizeof(cmd), 0) < 0) {
            return false;
        }
        return true;
    }

    void FoxThread::broadcast(callback cb) {
        dispatch(cb);
    }

    void FoxThread::stop() {
        RWMutex::WriteLock lock(m_mutex);
        m_callbacks.push_back(nullptr);
        if (m_thread) {
            uint8_t cmd = 0;
            send(m_write, &cmd, sizeof(cmd), 0);
        }
    }

    void FoxThread::join() {
        if (m_thread) {
            m_thread->join();
            delete m_thread;
            m_thread = NULL;
        }
    }

    FoxThread::~FoxThread() {
        if (m_read) {
            close(m_read);
        }
        if (m_write) {
            close(m_write);
        }
        stop();
        join();
        if (m_thread) {
            delete m_thread;
        }
        if (m_event) {
            event_free(m_event);
        }
        if (m_base) {
            event_base_free(m_base);
        }
    }

};

