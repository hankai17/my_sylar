#ifndef __REDIS_HH__
#define __REDIS_HH__

#include <memory>

#include "my_sylar/db/fox_thread.hh"
#include "my_sylar/scheduler.hh"

namespace sylar {
    typedef std::shared_ptr<redisReply> ReplyPtr;
    class IRedis {
    public:
        typedef std::shared_ptr<IRedis> ptr;
        enum Type {
            REDIS = 1,
            REDIS_CLUSTER = 2,
            FOX_REDIS = 3,
            FOX_REDIS_CLUSTER = 4
        };
        IRedis() : m_logEnable(true) {};
        ~virtual IRedis() {}

        virtual ReplyPtr cmd(const char* fmt, ...) = 0;
        virtual ReplyPtr cmd(const char* fmt, va_list ap) = 0;
        virtual ReplyPtr cmd(const std::vector<std::string>& argv) = 0;

        const std::string& getName() const { return m_name; }
        void setName(const std::string& name) { m_name = name; }
        Type getType() const { return m_type; }

    private:
    protected:
        std::string         m_name;
        Type                m_type;
        bool                m_logEnable;
    };

    class ISyncRedis : public IRedis {
    public:
        typedef std::shared_ptr<ISyncRedis> ptr;
        virtual ~ISyncRedis() {}

        virtual bool reconnect() = 0;
        virtual bool connect(const std::string& ip, int port, uint64_t ms = 0) = 0;
        virtual bool connect() = 0;
        virtual bool setTimeout(uint64_t ms) = 0;

        virtual int appendCmd(const char* fmt, ...) = 0;
        virtual int appendCmd(const char* fmt, va_list ap) = 0;
        virtual int appendCmd(const std::vector<std::string>& argv) = 0;

        virtual ReplyPtr getReply() = 0;
        uint64_t getLastActiveTime() { return m_lastActiveTime; }
        void setLastActiveTime(uint64_t time) { m_lastActiveTime = time; }

    protected:
        uint64_t            m_lastActiveTime;  
    };

    class Redis : public ISyncRedis {
    public:
        typedef std::shared_ptr<Redis> ptr;
        Redis();
        Redis(const std::map<std::string, std::string>& conf);

        virtual bool reconnect();
        virtual bool connect(const std::string& ip, int port, uint64_t ms = 0);
        virtual bool connect();
        virtual bool setTimeout(uint64_t ms);

        virtual int appendCmd(const char* fmt, ...);
        virtual int appendCmd(const char* fmt, va_list ap);
        virtual int appendCmd(const std::vector<std::string>& argv);

        virtual ReplyPtr getReply();

    private:
        std::string         m_host;
        uint32_t            m_port;
        uint32_t            m_connectMs;
        struct timeval      m_cmdTimeout;
        std::shared_ptr<redisContext>   m_context;
    };

    class FoxRedis : public IRedis {
    public:
        typedef std::shared_ptr<FoxRedis> ptr;
        enum STATUS {
            UNCONNECTED = 0,
            CONNECTING  = 1,
            CONNECTED   = 2
        };

        enum RESULT {
            OK          = 0,
            TIMEOUT     = 1,
            CONNECT_ERR = 2,
            CMD_ERR     = 3,
            REPLY_NULL  = 4,
            REPLY_ERR   = 5,
            INI_ERR     = 6
        };

        FoxRedis(sylar::FoxThread* thr, const std::map<std::string, std::string>& conf);
        ~FoxRedis();

        virtual ReplyPtr cmd(const char* fmt, ...);
        virtual ReplyPtr cmd(const char* fmt, va_list ap);
        virtual ReplyPtr cmd(const std::vector<std::string>& argv);

        bool init();
        int getCtxCount() const { return m_ctxCount; }

    private:
        struct FCtx {
            //typedef std::shared_ptr<FCtx> ptr;
            std::string         cmd;
            sylar::Scheduler*   scheduler;
            sylar::Fiber::ptr   fiber;
            ReplyPtr            rpy;
        };

        struct Ctx {
            typedef std::shared_ptr<Ctx> ptr;
            event*      ev;
            bool        timeout;
            FoxRedis*   rds;
            std::string cmd;
            FCtx*       fctx;
            FoxThread*  thread;

            Ctx(FoxRedis* rds);
            ~Ctx();
            bool init();
            void cancelEvent();
            static void EventCb(int fd, short event, void* d);
        };

    private:
        virtual void pcmd(FCtx* ctx);
        bool pinit();
        void delayDelete(redisAsyncContext* c);

    private:
        static void ConnectCb(const redisAsyncContext* c, int status);
        static void DisconnectCb(const redisAsyncContext* c, int status);
        static void CmdCb(redisAsyncContext* c, void* r, void* private);
        static void TimeCb(int fd, short event, void* d);

    private:
        sylar::FoxThread*       m_thread;
        std::shared_ptr<redisAsyncContext> m_context;
        std::string             m_host;
        uint16_t                m_port;
        STATUS                  m_status; 
        int                     m_ctxCount;
        struct timeval          m_cmdTimeout;
        std::string             m_err;
        struct event*           m_event;
    };

};
#endif

