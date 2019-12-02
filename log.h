//
// Created by root on 12/1/19.
//

#ifndef MY_SYLAR_LOG_H
#define MY_SYLAR_LOG_H

#include<memory>
#include<list>

namespace slylar {

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
       void ToString(LogLevel::level) {}
    };

    //(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, sylar::GetThreadId(),\
                sylar::GetFiberId(), time(0)))).getSS() // Just no idea
    class LogEvent {
        typedef std::shared_ptr<LogEvent> ptr;
    public:
        LogEvent(const char* file, uint64_t time, uint32_t line, uint32_t tid, uint32_t fid);
        const char* getFile() const { return filename; };
        uint64_t getTime() const { return m_time; };
        uint32_t getLine() const { return m_line; };
        uint32_t getThreadId() const { return m_threadId; };
        uint32_t getFiberId() const { return m_fiberId; };
        std::stringstream& getSS() const { return m_ss; }
        std::string getContent() const { return m_ss.str(); }

        void format(const char* fmt, ...);
        void format(const char* fmt, va_list al);
    private:
        const char* filename = nullptr; // Why not string
        uint64_t m_time         = 0;
        uint32_t m_line         = 0;
        uint32_t m_threadId     = 0;
        uint32_t m_fiberId      = 0;
        std::stringstream       m_ss;
    };

    class LogFormatter {
    public:
        typedef std::shared_ptr<LogFormatter> ptr;
        LogFormatter(std::string& pattern)
        : m_pattern(pattern) {
           init();
        }

        class FormatItem {
        public:
            typedef std::shared_ptr<FormatItem> ptr;
            virtual ~FormatItem() {};
            virtual void format(std::ostream& os, std::shard_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
        };

    private:
        std::string m_pattern;
        std::vector<FormatItem::ptr> m_items;
    };

    class LogAppender { // Where to log
    public:
        typedef std::shared_ptr<LogAppender> ptr;
        virtual ~LogAppender() {}; // This class Must be publiced, so use virtual deconstruct
    private:

    };

    class StdoutLogAppender : public LogAppender {
    public:
        typedef std::shared_ptr<StdoutLogAppender> ptr;
    };

    class FileLogAppender : public LogAppender {
    public:
        typedef std::shared_ptr<FileLogAppender> ptr;
    };

    class Logger { // User normal use. Publish it
    public:
        typedef std::shared_ptr<Logger> ptr;
        Logger(const std::string& name = "root");
        //void log(LogLevel::Level level, const std::string& filename, uint32_t line_no, uint64_t m_time); // How to set formatter?  so ugly so enclosure a class
        void log(LogLevel::Level level, LogEvent::ptr event); // user how to use it. //Why ptr
        std::string getName() const { return m_name; }

    private:
        std::string                 m_name;
        LogLevel::Level             m_level;
        std::list<LogAppender::ptr> m_appenders; // Use ptr
        LogFormatter::ptr           m_formatter;
    };

}

#endif //MY_SYLAR_LOG_H

/*
log --- x_appender --- formatter
                     |-- level
*/
