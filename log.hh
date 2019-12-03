//
// Created by root on 12/1/19.
//

#ifndef _LOG_HH_
#define _LOG_HH_

#include <memory>
#include <string>
#include <stdint.h>
#include <stdarg.h>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

namespace slylar {

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
       const char* ToString(LogLevel::Level level);
    };

    class LogEvent {
    public:
        typedef std::shared_ptr<LogEvent> ptr;
        LogEvent(uint64_t time, LogLevel::Level level, char* file, uint32_t line, uint32_t tid, uint32_t fid);
        uint64_t getTime() const { return m_time; }
        LogLevel::Level getLevel() const { return m_level; }
        const char* getFile() const { return filename; }
        uint32_t getLine() const { return m_line; }
        uint32_t getThreadId() const { return m_threadId; }
        uint32_t getFiberId() const { return m_fiberId; }
        std::stringstream& getSS() { return m_ss; } // Why not const? Why return &?
        std::string getContent() const { return m_ss.str(); }

        void format(const char* fmt, ...); // Init ss
        void format(const char* fmt, va_list al);
    private:
        const char* filename = nullptr; // Why not string // Below vaule all from para
        uint64_t m_time         = 0;
        uint32_t m_line         = 0;
        uint32_t m_threadId     = 0;
        uint32_t m_fiberId      = 0;
        std::stringstream       m_ss; // Why need this? We already had formatter
        LogLevel::Level         m_level;
    };

    class LogFormatter { // This class used for log4j config
    public:
        typedef std::shared_ptr<LogFormatter> ptr;
        LogFormatter(std::string& pattern)
        : m_pattern(pattern) {
           init(); // Parse log4j config then put every parsed item into m_items
        }
        void init();
        //std::string format(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event); // Out put log // Why not ???
        std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, std::shared_ptr<LogEvent> event); // Out put log
        class FormatItem {
        public:
            typedef std::shared_ptr<FormatItem> ptr;
            virtual ~FormatItem() {};
            virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, std::shared_ptr<LogEvent> event);
        };

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
    private:
        LogLevel::Level     m_level;
        LogFormatter::ptr   m_formatter;
    };

    class Logger { // User normal use. Publish it
    public:
        typedef std::shared_ptr<Logger> ptr;
        Logger(const std::string& name = "root");
        //void log(LogLevel::Level level, const std::string& filename, uint32_t line_no, uint64_t m_time); // How to set formatter?  so ugly so enclosure a class
        void log(LogLevel::Level level, LogEvent::ptr event); // user how to use it. //Why ptr
        std::string getName() const { return m_name; }

        void addAppender(LogAppender::ptr appender);
        void delAppender(LogAppender::ptr appender);

    private:
        std::string                 m_name;
        LogLevel::Level             m_level;
        std::list<LogAppender::ptr> m_appenders; // Use ptr
        LogFormatter::ptr           m_formatter;
    };

    class StdoutLogAppender : public LogAppender {
    public:
        typedef std::shared_ptr<StdoutLogAppender> ptr;
        void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event);
    };

    class FileLogAppender : public LogAppender {
    public:
        typedef std::shared_ptr<FileLogAppender> ptr;
        FileLogAppender(const std::string& filename);
        void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event);
        bool reopen();

    private:
        std::string     m_filename;
        std::ofstream   m_filestream;
    };

}

#endif

/*
logger
  |
  +--- formatter: parse log4j conf then collect every items
  |
  +--- appender:  How to use?

*/
