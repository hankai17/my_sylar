//
// Created by root on 12/1/19.
//

#include "log.h"

namespace sylar {
    LogEvent::LogEvent(const char* file, uint64_t time, uint32_t line,
                       uint32_t tid, uint32_t fid)
            : m_file(file),
              m_time(time),
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

    //item{format} item item item
    //ymdhms tid fid [level] [name] filename:lineno msg format
    //m, MessageFormatItem),
    //p, LevelFormatItem),
    //r, ElapseFormatItem),
    //c, NameFormatItem),
    //t, ThreadIdFormatItem),
    //n, NewLineFormatItem),
    //d, DateTimeFormatItem),
    //f, FilenameFormatItem),
    //l, LineFormatItem),
    //T, TabFormatItem),
    //F, FiberIdFormatItem),
    void LogFormatter::init() {
        std::vector<std::tuple<std::string, std::string, int> > vec; // str[1] {format}
        enum parse_stat {
            INIT = 0,
            ITEM = 1,
            FORM = 2,
            BAD  = 3,
            SUC  = 4
        };

        std::string item = '';
        std::string format = '';
        char* format_s = nullptr;
        char* format_e = nullptr;
        int type = 0;
        parse_stat curr_state = INIT;
        char word = '';

        for (size_t i = 0; i < m_pattern.size(); i++) { //%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n
            switch(curr_state) {
                case INIT:
                    if (m_pattern[i] == '%') {
                       curr_state = ITEM;
                        continue;
                    } else {
                        curr_state = INIT;
                        continue;
                    }
                case ITEM:
                    if (m_pattern[i] /*&& not space*/) { //TODO check vail
                        if (item == '') {
                            item = m_pattern[i];
                        }

                        if (m_pattern[i] == '{') {
                            curr_state = FORM;
                            format_s = &m_pattern[i];
                        }

                        if ( i + 1 < m_pattern.size() && m_pattern[i + 1] == '%') {
                            curr_state = SUC;
                        }
                       continue;
                    }
                    curr_state = BAD;
                    break;
                case FORM:
                    if (m_pattern[i] == '}') {
                        format_e = &m_pattern[i];
                        format = string(format_s + 1, format_s - format_e + 1);
                        curr_state = SUC;
                    }
                    continue;
                case SUC:
                    vec.insert(item, format, 1)
                    curr_state = INIT;
            }
        }

    }















    void LogFormatter::init() {

        std::vector<std::tuple<std::string, std::string, int> > vec;
        std::string nstr;
        for(size_t i = 0; i < m_pattern.size(); ++i) {
            if(m_pattern[i] != '%') {
                nstr.append(1, m_pattern[i]);
                continue;
            }

            if((i + 1) < m_pattern.size()) {
                if(m_pattern[i + 1] == '%') {
                    nstr.append(1, '%');
                    continue;
                }
            }

            size_t n = i + 1;
            int fmt_status = 0;
            size_t fmt_begin = 0;

            std::string str;
            std::string fmt;
            while(n < m_pattern.size()) {
                if(!fmt_status && (!isalpha(m_pattern[n]) && m_pattern[n] != '{'
                                   && m_pattern[n] != '}')) {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    break;
                }
                if(fmt_status == 0) {
                    if(m_pattern[n] == '{') {
                        str = m_pattern.substr(i + 1, n - i - 1);
                        //std::cout << "*" << str << std::endl;
                        fmt_status = 1; //解析格式
                        fmt_begin = n;
                        ++n;
                        continue;
                    }
                } else if(fmt_status == 1) {
                    if(m_pattern[n] == '}') {
                        fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                        //std::cout << "#" << fmt << std::endl;
                        fmt_status = 0;
                        ++n;
                        break;
                    }
                }
                ++n;
                if(n == m_pattern.size()) {
                    if(str.empty()) {
                        str = m_pattern.substr(i + 1);
                    }
                }
            }

            if(fmt_status == 0) {
                if(!nstr.empty()) {
                    vec.push_back(std::make_tuple(nstr, std::string(), 0));
                    nstr.clear();
                }
                vec.push_back(std::make_tuple(str, fmt, 1));
                i = n - 1;
            } else if(fmt_status == 1) {
                std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
                vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
            }
        }

        if(!nstr.empty()) {
            vec.push_back(std::make_tuple(nstr, "", 0));
        }
        static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)> > s_format_items = {
#define XX(str, C) \
        {#str, [](const std::string& fmt) { return FormatItem::ptr(new C(fmt));}}

                XX(m, MessageFormatItem),
                XX(p, LevelFormatItem),
                XX(r, ElapseFormatItem),
                XX(c, NameFormatItem),
                XX(t, ThreadIdFormatItem),
                XX(n, NewLineFormatItem),
                XX(d, DateTimeFormatItem),
                XX(f, FilenameFormatItem),
                XX(l, LineFormatItem),
                XX(T, TabFormatItem),
                XX(F, FiberIdFormatItem),
#undef XX
        };

        for(auto& i : vec) {
            if(std::get<2>(i) == 0) {
                m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
            } else {
                auto it = s_format_items.find(std::get<0>(i));
                if(it == s_format_items.end()) {
                    m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                } else {
                    m_items.push_back(it->second(std::get<1>(i)));
                }
            }

            //std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ")" << std::endl;
        }
        //std::cout << m_items.size() << std::endl;
    }

    Logger(const std::string& name)
            : m_name(name),
              m_leven(LogLevel::DEBUG) {
        m_formatter
    }
}
