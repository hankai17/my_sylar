#ifndef __FOX_THREAD__
#define __FOX_THREAD__

#include <memory>
#include <functional>
#include <thread>
#include <vector>
#include <list>
#include <map>
#include <thread>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>

#include "my_sylar/thread.hh"

namespace sylar {
    class IFoxThread {
    public:
        typedef std::shared_ptr<IFoxThread> ptr;
        typedef std::function<void()> callback;

        virtual ~IFoxThread() {};
        virtual bool dispatch(callback cb) = 0;
        virtual bool dispatch(uint32_t id, callback cb) = 0;
        virtual bool batchDispatch(const std::vector<callback>& cbs) = 0;
        virtual void broadcast(callback cb) = 0;

        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void join() = 0;
        virtual void dump(std::ostream& os) = 0;
        virtual uint64_t getTotal() = 0;
    };

    class FoxThread : public IFoxThread { // 就是一个 可调对象容器 + ev事件系统 只要有dispach就会触发回调 // 就是对ev的包装 + pipe回调系统  pipe相当于给base开了个生产口子
    public:
        typedef std::shared_ptr<FoxThread> ptr;
        typedef IFoxThread::callback callback;
        typedef std::function<void (FoxThread*)> init_cb;

        FoxThread(const std::string& name = "", struct event_base* base = NULL);
        ~FoxThread();

        static FoxThread* GetThis(); 
        static const std::string& GetFoxThreadName();
        static void GetAllFoxThreadName(std::map<uint64_t, std::string>& names);

        void setThis();
        void unsetThis();

        virtual bool dispatch(callback cb);
        virtual bool dispatch(uint32_t id, callback cb);
        virtual bool batchDispatch(const std::vector<callback>& cbs);
        virtual void broadcast(callback cb);

        void start();
        void stop();
        void join();

        bool isStart() const { return m_start; }
        struct event_base* getBase() const { return m_base; }
        std::thread::id getId() const;
        void* getData(const std::string& name);

        template<typename T>
        T* getData(const std::string& name) {
            return (T*)getData(name);
        }

        void setData(const std::string& name, void* v);

        void setInitCb(init_cb v) { m_initCb = v; }
        void dump(std::ostream& os);
        virtual uint64_t getTotal() { return m_total; }


    private:
        void thread_cb();
        static void read_cb(evutil_socket_t sock, short which, void* arg);

    private:
        evutil_socket_t         m_read;
        evutil_socket_t         m_write;
        struct event_base*      m_base;
        struct event*           m_event;
        std::thread*            m_thread;
        sylar::RWMutex          m_mutex;
        std::list<callback>     m_callbacks;
        std::string             m_name;
        init_cb                 m_initCb;
        std::map<std::string, void*> m_datas;
        bool                    m_working;
        bool                    m_start;
        uint64_t                m_total;
    };

}

#endif

