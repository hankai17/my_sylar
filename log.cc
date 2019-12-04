//
// Created by root on 12/1/19.
//

#include "log.hh"
#include <map>
#include <iostream>
#include <functional>
#include <time.h>
#include <string.h>

namespace sylar {

    const char* LogLevel::ToString(LogLevel::Level level) {
        switch (level) {
#define XX(name) \
            case LogLevel::name: \
                return #name; \
                break;
            XX(DEBUG);
            XX(INFO);
            XX(WARN);
            XX(ERROR);
            XX(FATAL);
#undef XX
            default:
                return "UNKNOWN";
        }
        return "UNKNOWN";
    }

    LogEvent::LogEvent(uint64_t time, LogLevel::Level level, const char* file, uint32_t line,
                       uint32_t tid, uint32_t fid)
            :m_time(time),
             m_level(level),
             m_file(file),
             m_line(line),
             m_threadId(tid),
             m_fiberId(fid) {
    }

    void LogEvent::format(const char* fmt, ...) {
        va_list al;
        va_start(al, fmt);
        format(fmt, al);
        va_end(al);
    }

    void LogEvent::format(const char* fmt, va_list al) {
        char* buf = nullptr;
        int len = vasprintf(&buf, fmt, al);
        if (len != -1) {
            m_ss << std::string(buf, len);
            free(buf);
        }
    }

    class NameFormatItem : public LogFormatter::FormatItem {
    public:
        NameFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << logger->getName();
        }
    };

    class LevelFormatItem : public LogFormatter::FormatItem {
    public:
        LevelFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << LogLevel::ToString(level);
        }
    };

    class FilenameFormatItem : public LogFormatter::FormatItem {
    public:
        FilenameFormatItem(const std::string& str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << event->getFile();
        }
    };

    class DataTimeFormatItem : public LogFormatter::FormatItem {
    public:
        DataTimeFormatItem(const std::string& str = "%Y-%m-%d %H:%M:%S")
                : m_format(str) {
            if (m_format.empty()) {
                m_format = "%Y-%m-%d %H:%M:%S";
            }
        }
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            struct tm tm;
            time_t time = event->getTime();
            localtime_r(&time, &tm);
            char buf[64];
            strftime(buf, sizeof(buf), m_format.c_str(), &tm);
            os << buf;
        }
    private:
        std::string m_format;
    };

    class LineFormatItem : public LogFormatter::FormatItem {
    public:
        LineFormatItem(const std::string& str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << event->getLine();
        }
    };

    class ThreadIdFormatItem : public LogFormatter::FormatItem {
    public:
        ThreadIdFormatItem(const std::string& str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << event->getThreadId();
        }
    };

    class FiberIdFormatItem : public LogFormatter::FormatItem {
    public:
        FiberIdFormatItem(const std::string& str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << event->getFiberId();
        }
    };

    class MessageFormatItem : public LogFormatter::FormatItem {
    public:
        MessageFormatItem(const std::string& str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << event->getContent();
        }
    };

    class NewLineFormatItem : public LogFormatter::FormatItem {
    public:
        NewLineFormatItem(const std::string& str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << std::endl;
        }
    };

    class TabFormatItem : public LogFormatter::FormatItem {
    public:
        TabFormatItem(const std::string& str = "") {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << "\t";
        }
    };

    class StringFormatItem : public LogFormatter::FormatItem {
    public:
        StringFormatItem(const std::string& str)
                : m_string(str) {}
        void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
            os << m_string;
        }
    private:
        std::string m_string;
    };

    std::string LogFormatter::format(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
        std::stringstream ss;
        for (auto&i : m_items) {
            i->format(ss, logger, level, event);
        }
        return ss.str();
    }

    LogFormatter::LogFormatter(const std::string& pattern)
            : m_pattern(pattern) {
        init();
    }

    void StdoutLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
        if (level >= m_level) {
            std::cout <<  m_formatter->format(logger, level, event);
        }
    }

    FileLogAppender::FileLogAppender(const std::string& filename)
    :m_filename(filename) {
       reopen();
    }

    void FileLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
        if (level >= m_level) {
            m_filestream << m_formatter->format(logger, level, event);
        }
    }

    bool FileLogAppender::reopen() {
        if (m_filestream) {
            m_filestream.close();
        }
        m_filestream.open(m_filename, std::ios::app);
        return !!m_filestream;
    }

    void Logger::addAppender(LogAppender::ptr appender) {
        if (!appender->getFormatter()) {
            appender->setFormatter(m_formatter);
        }
        m_appenders.push_back(appender);
    }

    void Logger::delAppender(LogAppender::ptr appender) {
        for (auto it = m_appenders.begin(); it != m_appenders.end(); it++) {
            if (*it == appender) {
                m_appenders.erase(it);
                break;
            }
        }
        /*
        for (auto it = m_appenders.begin(); it != m_appenders.end();) {
            if (*it == appender) {
                m_appenders.erase(it++);
                break;
            } else {
                it++;
            }
        }
         */
    }

    Logger::Logger(const std::string& name)
            : m_name(name),
              m_level(LogLevel::DEBUG) {
        m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
    }

    void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
        if (level >= m_level) {
            for (auto&i : m_appenders) {
                //i->log(Logger::ptr(this), level, event);
                i->log(std::make_shared<Logger>(*this), level, event);
            }
        }
    }

    void LogFormatter::init() {
        std::vector<std::tuple<std::string, std::string, int> > vec;
        enum parse_stat { // Do not design deeply
            INIT = 0,
            ITEM = 1,
            FORM = 2,
            NORM = 3,
        };

        std::string item;
        std::string format;
        char* format_s = nullptr;
        char* format_e = nullptr;
        parse_stat curr_state = INIT;

        for (size_t i = 0; i < m_pattern.size(); i++) { //%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n
            //std::cout<<curr_state<<" "<< m_pattern[i]<<std::endl;
            switch(curr_state) {
                case INIT:
                    if (m_pattern[i] == '%') {
                        curr_state = ITEM;
                    } else {
                        curr_state = INIT;
                    }
                    break;
                case ITEM:
                    if (m_pattern[i] /*&& not space*/) { //TODO check vail
                        if (item == "") {
                            item = m_pattern[i];
                            if ( i + 1 < m_pattern.size() && m_pattern[i + 1] != '{') {
                                vec.push_back(std::make_tuple(item, format, 1));
                                item = "";
                                format = "";
                                curr_state = INIT;
                            } else if (i + 1 < m_pattern.size() && m_pattern[i + 1] == '{') {
                                format_s = &m_pattern[i + 1];
                                curr_state =  FORM;
                            } else if ( i == m_pattern.size() - 1) {
                                vec.push_back(std::make_tuple(item, format, 1));
                                item = "";
                                format = "";
                                curr_state = INIT;
                            }
                            if (i + 1 < m_pattern.size() &&
                                (m_pattern[i + 1] == '[' || m_pattern[i + 1] == ']' || m_pattern[i + 1] == ':') ) { //special str: []
                                curr_state = NORM;
                            }
                        }
                        break;
                    }
                    break;
                case FORM:
                    if (m_pattern[i] == '}') {
                        format_e = &m_pattern[i];
                        format = std::string(format_s + 1, format_e - format_s - 1);
                        vec.push_back(std::make_tuple(item, format, 1));
                        item = "";
                        format = "";
                        curr_state = INIT;
                    }
                    break;
                case NORM:
                    // special char
                    vec.push_back(std::make_tuple(std::string(&m_pattern[i], 1), format, 0));
                    item = "";
                    format = "";
                    curr_state = INIT;
                    break;
                default:
                    break;
            }
        }

        // register factory map
        static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)> > s_format_items = {
#define XX(str, C) \
        {#str, [](const std::string& fmt) { return FormatItem::ptr (new C(fmt)); }}

                XX(m, MessageFormatItem),
                XX(p, LevelFormatItem),
                XX(c, NameFormatItem),
                XX(t, ThreadIdFormatItem),
                XX(n, NewLineFormatItem),
                XX(d, DataTimeFormatItem),
                XX(f, FilenameFormatItem),
                XX(l, LineFormatItem),
                XX(T, TabFormatItem),
                XX(F, FiberIdFormatItem),
#undef XX
        };

        for (auto& i : vec) {
            //std::cout << std::get<0>(i) << std::endl;
            if (std::get<2>(i) == 0) {
                m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i)))); // "["
            } else {
                auto it = s_format_items.find(std::get<0>(i));
                if (it != s_format_items.end()) {
                    m_items.push_back(it->second(std::get<1>(i)));
                } else {
                    m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error format %" + std::get<0>(i) + ">>")));
                }
            }
        }
    }
}
