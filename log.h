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
    };

    class log {

    };

    class LogFormatter {
    public:
        typedef std::shared_ptr<LogFormatter> ptr;
    private:

    };

    class LogAppender { // Where to log
    public:
        typedef std::shared_ptr<LogAppender> ptr;
        virtual ~LogAppender() {}; // This class Must be publiced, so use virtual deconstruct
    };

    class Logger {
    public:
        typedef std::shared_ptr<Logger> ptr;
    private:
        std::string                 m_name;
        LogLevel::Level             m_level;
        std::list<LogAppender::ptr> m_appenders; // Use ptr
        LogFormatter::ptr           m_formatter;
    };

}

#endif //MY_SYLAR_LOG_H
