#include "my_sylar/db/redis.hh"
#include "my_sylar/log.hh"
#include "my_sylar/util.hh"

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

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

    Redis::reconnect() {
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
            m_context.reset(c, redisFree); // why not put into deconstruct?
            return true;
        }
        return false;
    }

    bool Redis::setTimeout(uint64_t ms) {
        m_cmdTimeout.tv_sec = ms / 1000;
        m_cmdTimtout.tv_usec = ms % 1000 * 1000;
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
        
       
    }

};

