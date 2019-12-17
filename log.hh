//
// Created by root on 12/1/19.
//

#ifndef _LOG_HH_
#define _LOG_HH_

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <string>
#include <fstream>
#include <vector>
#include <stdarg.h>
#include <map>
#include <yaml-cpp/yaml.h>
#include "util.hh"
#include "thread.hh"

#define SYLAR_LOG_LEVEL(logger, level) \
    if (logger->getLevel() <= level) \
        sylar::LogEventWrap(logger, \
                sylar::LogEvent::ptr(new sylar::LogEvent(time(0), level, __FILE__, __LINE__, sylar::GetThreadId(), sylar::GetFiberId()))).getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if (logger->getLevel() <= level) \
        sylar::LogEventWrap(logger, \
        sylar::LogEvent::ptr(new sylar::LogEvent(time(0), level, __FILE__, __LINE__, sylar::GetThreadId(), sylar::GetFiberId()))).getLogEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel:DEBUG, fmt, ...)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel:INFO, fmt, ...)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel:WARN, fmt, ...)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel:ERROR, fmt, ...)

#define SYLAR_LOG_ROOT() sylar::LoggerManager::getLogMgr()->getRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerManager::getLogMgr()->getLogger(name)

namespace sylar {

    class Logger;

    class LogLevel {
    public:
        enum Level {
            UNKNOW  = 0,
            DEBUG   = 1,
            INFO    = 2,
            WARN    = 3,
            ERROR   = 4,
            FATAL   = 5
        };
       static const char* ToString(LogLevel::Level level);
       static LogLevel::Level FromString(const std::string& val);
    };

    class LogEvent {
    public:
        typedef std::shared_ptr<LogEvent> ptr;
        LogEvent(uint64_t time, LogLevel::Level level, const char* file, uint32_t line, uint32_t tid, uint32_t fid);
        uint64_t getTime() const { return m_time; }
        LogLevel::Level getLevel() const { return m_level; }
        const char* getFile() const { return m_file; }
        uint32_t getLine() const { return m_line; }
        uint32_t getThreadId() const { return m_threadId; }
        uint32_t getFiberId() const { return m_fiberId; }
        std::stringstream& getSS() { return m_ss; } // Why not const? Why return &?
        std::string getContent() const { return m_ss.str(); }

        void format(const char* fmt, ...); // Init ss
        void format(const char* fmt, va_list al);
    private:
        uint64_t m_time         = 0;
        LogLevel::Level         m_level;
        const char* m_file      = nullptr; // Why not string // Below vaule all from para
        uint32_t m_line         = 0;
        uint32_t m_threadId     = 0;
        uint32_t m_fiberId      = 0;
        std::stringstream       m_ss; // Why need this? We already had formatter
    };

    class LogFormatter { // This class used for log4j config
    public:
        typedef std::shared_ptr<LogFormatter> ptr;
        LogFormatter(const std::string& pattern);
        //std::string format(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event); // Out put log // Why not ???
        std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, std::shared_ptr<LogEvent> event); // Out put log
        std::string getFormatter() const { return m_pattern; }
        std::string toYamlString();
    public:
        class FormatItem {
        public:
            typedef std::shared_ptr<FormatItem> ptr;
            virtual ~FormatItem() {};
            virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, std::shared_ptr<LogEvent> event) = 0; // Pure virtual func
        };
        void init();

    private:
        std::string m_pattern;
        std::vector<FormatItem::ptr> m_items;
    };

    class LogAppender { // Where to log
    public:
        typedef std::shared_ptr<LogAppender> ptr;
        virtual ~LogAppender() {}; // This class Must be publiced, so use virtual deconstruct

        void setFormatter(LogFormatter::ptr val) { m_formatter = val; }
        void setLevel(LogLevel::Level l) { m_level = l; }
        LogFormatter::ptr getFormatter() const { return m_formatter; }
        LogLevel::Level getLevel() const { return m_level; }
        virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
        virtual std::string toYamlString() = 0;
        virtual std::string getType() = 0;
        virtual std::string getFile() = 0;
        virtual void setType(const std::string& type) = 0;
        virtual void setFile(const std::string& file) = 0;

    protected:
        LogLevel::Level     m_level;
        LogFormatter::ptr   m_formatter;
        std::string         m_type;
        std::string         m_file;
    };

    class Logger { // User normal use. Publish it
    public:
        typedef std::shared_ptr<Logger> ptr;
        typedef sylar::Mutex MutexType;
        //void log(LogLevel::Level level, const std::string& filename, uint32_t line_no, uint64_t m_time); // How to set formatter?  so ugly so enclosure a class
        void log(LogLevel::Level level, LogEvent::ptr event); // user how to use it. //Why ptr
        std::string getName() const { return m_name; }
        LogLevel::Level getLevel() const { return m_level; }

        void addAppender(LogAppender::ptr appender);
        void delAppender(LogAppender::ptr appender);
        Logger(const std::string& name = "root");

    private:
        std::string                 m_name;
        LogLevel::Level             m_level;
        std::list<LogAppender::ptr> m_appenders; // Use ptr
        LogFormatter::ptr           m_formatter;
        MutexType                   m_mutex;
    };

    class StdoutLogAppender : public LogAppender {
    public:
        typedef std::shared_ptr<StdoutLogAppender> ptr;
        void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
        std::string toYamlString() override;
        std::string getType() override { return "StdoutLogAppender"; }
        std::string getFile() override { return ""; }
        void setType(const std::string& val) override { m_type = "StdoutLogAppender"; }
        void setFile(const std::string& val) override {}
    };

    class FileLogAppender : public LogAppender {
    public:
        typedef std::shared_ptr<FileLogAppender> ptr;
        FileLogAppender(const std::string& filename);
        void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
        std::string toYamlString() override;
        bool reopen();
        std::string getFilename() const { return m_filename; }
        std::string getType() override { return "FileLogAppender"; };
        std::string getFile() override { return m_filename; }
        void setType(const std::string& val) override { m_type = "FileLogAppender"; }
        void setFile(const std::string& val) override { m_filename = val; }

    private:
        std::string     m_filename;
        std::ofstream   m_filestream;
    };

    class LogEventWrap { // a middle layer used to event and logger
    public:
        typedef std::shared_ptr<LogEventWrap> ptr;
        LogEventWrap(Logger::ptr logger, LogEvent::ptr event);
        LogEvent::ptr getLogEvent() const { return m_event; }
        std::stringstream& getSS() const { return m_event->getSS(); }
        ~LogEventWrap();

    private:
        Logger::ptr     m_logger;
        LogEvent::ptr   m_event;
    };

    class LoggerManager { // not design deeply // logger should not siglon logmanager should be siglon
    public:
        typedef std::shared_ptr<LoggerManager> ptr;
        Logger::ptr getLogger(const std::string& name);
        Logger::ptr getRoot() const { return m_root; }
        static LoggerManager* getLogMgr(); //{ return logMgr; }

    private:
        LoggerManager();
        static LoggerManager*               logMgr;
        Logger::ptr                         m_root;
        std::map<std::string, Logger::ptr>  m_loggers;
    };
    //LoggerManager* LoggerManager::logMgr = new LoggerManager;  // Why not here

    class LoggerConfig {
    public:
        typedef std::shared_ptr<LoggerConfig> ptr;

        std::string toYamlString() const {
            YAML::Node node;
            node["name"] = m_log_name;
            node["level"] = LogLevel::ToString(m_level);
            node["formatter"] = m_formatter;
            for (const auto& i : m_appenders) {
                node["appender"].push_back(YAML::Load(i->toYamlString())); // not node.push_back(YAML::Load(i->toYamlString))
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }

        std::string getLogName() const { return m_log_name; }
        void setLogName(const std::string& log_name) { m_log_name = log_name; }
        LogLevel::Level getLogLevel() const { return m_level; }
        void setLogLevel(const LogLevel::Level& level) { m_level = level; }
        std::string getFormatter() const { return m_formatter; }
        void setLogFormatter(const std::string& formatter) { m_formatter = formatter; }
        const std::vector<LogAppender::ptr>& getAppenders() const { return m_appenders; } // otherwise cant not insert! // this func can not insert
        void pushAppender(LogAppender::ptr p) { m_appenders.push_back(p); }

    private:
        std::string         m_log_name;
        LogLevel::Level     m_level;
        std::string         m_formatter;
        std::vector<LogAppender::ptr>   m_appenders;
    };
}

#endif

/*
logger
  |
  +--- formatter: parse log4j conf then collect every items
  |
  +--- appender:  How to use?

How to use?
1. alloc event
2. alloc appender to init logger
logger->log(level, event)
*/
