#include "my_sylar/db/redis.hh"
#include "my_sylar/log.hh"
#include "my_sylar/util.hh"
#include "my_sylar/hash.hh"
#include "my_sylar/macro.hh"

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static std::string get_value(const std::map<std::string, std::string>& m,
          const std::string& key, const std::string& def = "") {
        auto it = m.find(key);
        return it == m.end() ? def : it->second;
    }

    Redis::Redis() {
        m_type = IRedis::REDIS;
    }

    Redis::Redis(const std::map<std::string, std::string>& conf) {
        m_type = IRedis::REDIS;
        auto tmp = get_value(conf, "host"); // refer mysql
        auto pos = tmp.find(":");
        m_host = tmp.substr(0, pos);
        m_port = sylar::TypeUtil::Atoi(tmp.substr(pos + 1));
        m_logEnable = sylar::TypeUtil::Atoi(get_value(conf, "log_enable", "1"));

        tmp = get_value(conf, "timeout_com");
        if (tmp.empty()) {
            tmp = get_value(conf, "timeout");
        }
        uint64_t v = sylar::TypeUtil::Atoi(tmp);
        m_cmdTimeout.tv_sec = v / 1000;
        m_cmdTimeout.tv_usec = v % 1000 * 1000;
    }

    bool Redis::reconnect() {
        return redisReconnect(m_context.get());
    }

    bool Redis::connect() {
        return connect(m_host, m_port, 50); // why 50?
    }

    bool Redis::connect(const std::string& ip, int port, uint64_t ms) {
        m_host = ip;
        m_port = port;
        m_connectMs = ms;
        if (m_context) {
            return true;
        }

        timeval tv = { (int)ms / 1000, (int)ms % 1000 * 1000 };
        auto c = redisConnectWithTimeout(ip.c_str(), port, tv);
        if (c) {
            if (m_cmdTimeout.tv_sec || m_cmdTimeout.tv_usec) {
                setTimeout(m_cmdTimeout.tv_sec * 1000 + m_cmdTimeout.tv_usec / 1000);
            }
            m_context.reset(c, redisFree); // why not put into deconstruct? // 因为连接复用
            return true;
        }
        return false;
    }

    bool Redis::setTimeout(uint64_t ms) {
        m_cmdTimeout.tv_sec = ms / 1000;
        m_cmdTimeout.tv_usec = ms % 1000 * 1000;
        redisSetTimeout(m_context.get(), m_cmdTimeout);
        return true;
    }

    ReplyPtr Redis::cmd(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        ReplyPtr ret = cmd(fmt, ap);
        va_end(ap);
        return ret;
    }

    ReplyPtr Redis::cmd(const char* fmt, va_list ap) {
        auto r = (redisReply*)redisvCommand(m_context.get(), fmt, ap);
        if (!r) {
            if (m_logEnable) {
                SYLAR_LOG_ERROR(g_logger) << "redisCommand error: (" << fmt << ")(" 
                  << m_host << ":" << m_port << ")(" << m_name << ")";
            }
            return nullptr;
        }
        ReplyPtr ret(r, freeReplyObject);
        if (r->type != REDIS_REPLY_ERROR) {
            return ret;
        }
        if (m_logEnable) {
            SYLAR_LOG_ERROR(g_logger) << "redisCommand error: (" << fmt << ")(" 
              << m_host << ":" << m_port << ")(" << m_name << ") " << r->str;
        }
        return nullptr;
    }

    ReplyPtr Redis::cmd(const std::vector<std::string>& argv) {
        std::vector<const char*> v;
        std::vector<size_t> l;
        for (const auto& i : argv) {
            v.push_back(i.c_str());
            l.push_back(i.size());
        }
        auto r = (redisReply*)redisCommandArgv(m_context.get(), argv.size(), &v[0], &l[0]);
        if (!r) {
            if (m_logEnable) {
                SYLAR_LOG_ERROR(g_logger) << "redisCommandArgv error "
                  << m_host << ":" << m_port << ")(" << m_name << ")";
            }
            return nullptr;
        }
        ReplyPtr ret(r, freeReplyObject);
        if (r->type != REDIS_REPLY_ERROR) {
            return ret;
        }
        if (m_logEnable) {
            SYLAR_LOG_ERROR(g_logger) << "redisCommandArgv error " 
              << m_host << ":" << m_port << ")(" << m_name << ") " << r->str;
        }
        return nullptr;
    }

    ReplyPtr Redis::getReply() {
        redisReply* r = nullptr;
        if (redisGetReply(m_context.get(), (void**)&r) == REDIS_OK) {
            ReplyPtr ret(r, freeReplyObject);
            return ret;
        }
        if (m_logEnable) {
            SYLAR_LOG_ERROR(g_logger) << "redisCommand getReply error " 
              << m_host << ":" << m_port << ")(" << m_name << ") " << r->str;
        }
        return nullptr;
    }

    int Redis::appendCmd(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int ret = appendCmd(fmt, ap);
        va_end(ap);
        return ret;
    }

    int Redis::appendCmd(const char* fmt, va_list ap) {
        return redisvAppendCommand(m_context.get(), fmt, ap);
    }

    int Redis::appendCmd(const std::vector<std::string>& argv) {
        std::vector<const char* > v;
        std::vector<size_t> l;
        for (const auto& i : argv) {
            v.push_back(i.c_str());
            l.push_back(i.size());
        }
        return redisAppendCommandArgv(m_context.get(), argv.size(), &v[0], &l[0]);
    }


    FoxRedis::FoxRedis(sylar::FoxThread* thr, const std::map<std::string, std::string>& conf) :
      m_thread(thr),
      m_status(UNCONNECTED),
      m_event(nullptr) {
        m_type = IRedis::FOX_REDIS;
        auto tmp = get_value(conf, "host");
        auto pos = tmp.find(":");
        m_host = tmp.substr(0, pos);
        m_port = sylar::TypeUtil::Atoi(tmp.substr(pos + 1));
        m_ctxCount = 0;
        m_logEnable = sylar::TypeUtil::Atoi(get_value(conf, "log_enable", "1"));

        tmp = get_value(conf, "timeout_comm");
        if (tmp.empty()) {
            tmp = get_value(conf, "timeout");
        }

        uint64_t v = sylar::TypeUtil::Atoi(tmp);
        m_cmdTimeout.tv_sec = v / 1000;
        m_cmdTimeout.tv_usec = v % 1000 * 1000;
    }

    void FoxRedis::ConnectCb(const redisAsyncContext* c, int status) {
        FoxRedis* th = static_cast<FoxRedis*>(c->data);
        if (!status) {
            SYLAR_LOG_INFO(g_logger) << "FoxRedis::ConnectCb "
              << c->c.tcp.host << ":" << c->c.tcp.port
              << " success";
            th->m_status = CONNECTED;
        } else {
            SYLAR_LOG_INFO(g_logger) << "FoxRedis::ConnectCb "
              << c->c.tcp.host << ":" << c->c.tcp.port
              << " faild err: " << c->errstr;
            th->m_status = UNCONNECTED;
        }
    }

    void FoxRedis::DisconnectCb(const redisAsyncContext* c, int status) {
        SYLAR_LOG_INFO(g_logger) << "FoxRedis::DisconnectCb "
          << c->c.tcp.host << ":" << c->c.tcp.port
          << " status: " << status;
        FoxRedis* th = static_cast<FoxRedis*>(c->data);
        th->m_status = UNCONNECTED;
    }

    bool FoxRedis::init() {
        if (m_thread == sylar::FoxThread::GetThis()) {
            return pinit();
        } else {
            m_thread->dispatch(std::bind(&FoxRedis::pinit, this));
        }
        return true;
    }

    void FoxRedis::TimerCb(int fd, short event, void* d) {
        FoxRedis* th = static_cast<FoxRedis*>(d);
        redisAsyncCommand(th->m_context.get(), CmdCb, nullptr, "ping");
    }

    bool FoxRedis::pinit() {
        if (m_status != UNCONNECTED) {
            return true;
        }
        auto ctx = redisAsyncConnect(m_host.c_str(), m_port);
        if (!ctx) {
            SYLAR_LOG_ERROR(g_logger) << "redisAsyncConnect (" << m_host 
              << ":" << m_port << ") null"; 
            return false;
        }
        if (ctx->err) {
            SYLAR_LOG_ERROR(g_logger) << "Error: (" << ctx->err << ")"
              << ctx->errstr;
            return false;
        }

        ctx->data = this;
        redisLibeventAttach(ctx, m_thread->getBase());
        redisAsyncSetConnectCallback(ctx, ConnectCb);
        redisAsyncSetDisconnectCallback(ctx, DisconnectCb);

        m_status = CONNECTING;
        m_context.reset(ctx, sylar::nop<redisAsyncContext>); // Why not write emptry ? so strange
        //m_context.reset(ctx, redisAsyncFree);

        if (m_event == nullptr) {
            m_event = event_new(m_thread->getBase(), -1, EV_TIMEOUT | EV_PERSIST, TimerCb, this);
            struct timeval tv {120, 0};
            evtimer_add(m_event, &tv);
        }
        //TimerCb(0, 0, this); // Why need this ???
        return true;
    }

    ReplyPtr FoxRedis::cmd(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        auto ret = cmd(fmt, ap);
        va_end(ap);
        return ret;
    }

    void FoxRedis::pcmd(FCtx* fctx) {
        if (m_status == UNCONNECTED) {
            SYLAR_LOG_INFO(g_logger) << "redis(" << m_host << ":" << m_port << ") unconnected "
              << fctx->cmd;
            init();
            if (fctx->fiber) {
                fctx->scheduler->schedule(&fctx->fiber);
            }
            return;
        }

        Ctx* ctx(new Ctx(this)); // 这里是异步 分配ctx
        ctx->thread = m_thread;
        ctx->init();
        ctx->fctx = fctx;
        ctx->cmd = fctx->cmd;

        if (!ctx->cmd.empty()) {
            redisAsyncFormattedCommand(m_context.get(), CmdCb, ctx, ctx->cmd.c_str(), ctx->cmd.size());
        }
    }

    ReplyPtr FoxRedis::cmd(const char* fmt, va_list ap) {
        char* buff = nullptr;
        int len = redisvFormatCommand(&buff, fmt, ap);
        if (len == -1) {
            SYLAR_LOG_ERROR(g_logger) << "redis fmt failed: " << fmt;
            return nullptr;
        }

        FCtx fctx;
        fctx.cmd.append(buff, len);
        free(buff);
        fctx.scheduler = sylar::Scheduler::GetThis();
        fctx.fiber = sylar::Fiber::GetThis();
        m_thread->dispatch(std::bind(&FoxRedis::pcmd, this, &fctx)); // 因为是同步的 直接用栈fctx就可以
        sylar::Fiber::YeildToHold();
        return fctx.rpy;
    }

    ReplyPtr FoxRedis::cmd(const std::vector<std::string>& argv) {
        FCtx fctx;
        do {
            std::vector<const char*> args;
            std::vector<size_t> args_len;
            for (const auto& i : argv) {
                args.push_back(i.c_str());
                args_len.push_back(i.size());
            }

            char* buff = nullptr;
            int len = redisFormatCommandArgv(&buff, argv.size(), &args[0], &args_len[0]);
            if (len == -1 || !buff) {
                SYLAR_LOG_ERROR(g_logger) << "redis fmt failed";
                return nullptr;
            }
            fctx.cmd.append(buff, len);
            free(buff);
        } while(0);

        fctx.scheduler = sylar::Scheduler::GetThis();
        fctx.fiber = sylar::Fiber::GetThis();
        m_thread->dispatch(std::bind(&FoxRedis::pcmd, this, &fctx));
        sylar::Fiber::YeildToHold();
        return fctx.rpy;
    }

    void FoxRedis::CmdCb(redisAsyncContext* c, void* r, void* pridata) {
        Ctx* ctx = static_cast<Ctx*>(pridata);
        if (!ctx) {
            return;
        }

        if (ctx->timeout) {
            delete ctx;
            return;
        }

        auto m_logEnable = ctx->rds->m_logEnable;
        redisReply* reply = (redisReply*)r;
        if (c->err) {
            if (m_logEnable) {
                sylar::replace(ctx->cmd, "\r\n", "\\r\\n");
                SYLAR_LOG_ERROR(g_logger) << "redis cmd: " << ctx->cmd
                  << " c->err: " << c->errstr;
            }
            if (ctx->fctx->fiber) {
                ctx->fctx->scheduler->schedule(&ctx->fctx->fiber);
            }
        } else if (!reply) {
            if (m_logEnable) {
                sylar::replace(ctx->cmd, "\r\n", "\\r\\n");
                SYLAR_LOG_ERROR(g_logger) << "redis cmd: " << ctx->cmd
                  << " reply NULL ";
            }
            if (ctx->fctx->fiber) {
                ctx->fctx->scheduler->schedule(&ctx->fctx->fiber);
            }
        } else if (reply->type == REDIS_REPLY_ERROR) {
            if (m_logEnable) {
                sylar::replace(ctx->cmd, "\r\n", "\\r\\n");
                SYLAR_LOG_ERROR(g_logger) << "redis cmd: " << ctx->cmd
                  << " reply: " << reply->str;
            }
            if (ctx->fctx->fiber) {
                ctx->fctx->scheduler->schedule(&ctx->fctx->fiber);
            }
        }
        ctx->cancelEvent();
        delete ctx;
    }

    FoxRedis::~FoxRedis() {
        if (m_event) {
            evtimer_del(m_event);
            event_free(m_event);
        }
    }

    FoxRedis::Ctx::Ctx(FoxRedis* f) :
        ev(nullptr),
        timeout(false),
        rds(f),
        thread(nullptr) {
          sylar::Atomic::addFetch(rds->m_ctxCount, 1);
    }

    FoxRedis::Ctx::~Ctx() {
        SYLAR_ASSERT(thread == sylar::FoxThread::GetThis());
        sylar::Atomic::subFetch(rds->m_ctxCount, 1);
        if (ev) {
            evtimer_del(ev);
            event_free(ev);
            ev = nullptr;
        }
    }

    void FoxRedis::Ctx::cancelEvent() {
        
    }
    
    bool FoxRedis::Ctx::init() {
        ev = evtimer_new(rds->m_thread->getBase(), EventCb, this);
        evtimer_add(ev, &rds->m_cmdTimeout);
        return true;
    }

    void FoxRedis::Ctx::EventCb(int fd, short event, void* d) {
        Ctx* ctx = static_cast<Ctx*>(d);
        ctx->timeout = 1;
        if (ctx->rds->m_logEnable) {
            sylar::replace(ctx->cmd, "\r\n", "\\r\\n");
            SYLAR_LOG_INFO(g_logger) << "redis cmd: " << ctx->cmd
              << " reached Timeout " << ctx->rds->m_cmdTimeout.tv_sec * 1000 +
              (ctx->rds->m_cmdTimeout.tv_usec / 1000) << " ms";
        }

        if (ctx->fctx->fiber) {
            ctx->fctx->scheduler->schedule(&ctx->fctx->fiber);
        }
        ctx->cancelEvent();
    }

    // https://github.com/sylar-yin/sylar/commit/efab6d5d82ca30e945916569684dad99384d30a1

};

