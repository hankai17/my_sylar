//
// Created by root on 12/1/19.
//

#ifndef MY_SYLAR_LOG_H
#define MY_SYLAR_LOG_H

#include<memory>

namespace slylar {

    class LogLevel {
    public:
        enum Level {
            UNKNOW = 0,
            DEBUG = 1,
            INFO = 2,
            WARN = 3,
            ERROR = 4,
            FATAL = 5
        };
    };

    class log {

    };

    class Logger {
    public:
        typedef std::shared_ptr<Logger> ptr;
    private:
        std::string m_name;
        LogLevel::Level m_level;
        //output where
        //formatter //
    };

}

#endif //MY_SYLAR_LOG_H
