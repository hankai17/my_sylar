#include "http/http.hh"
#include "log.hh"

#include <string.h>

namespace sylar {
    namespace http {

        bool CaseInsensitiveLess::operator() (const std::string& lv,
                const std::string& rv) const {
            return strcasecmp(lv.c_str(), rv.c_str());
        }

        HttpRequest::HttpRequest(uint8_t version, bool close)
        : m_method(HttpMethod::GET),
        m_version(version),
        m_close(close) {
        }

        std::string HttpRequest::getPara(const std::string& key, const std::string& def) const {
            auto it = m_paras.find(key);
            if (it == m_paras.end()) {
                return def;
            }
            return it->second;
        }

        std::string HttpRequest::getHeader(const std::string& key, const std::string& def) const {
            auto it = m_headers.find(key);
            if (it == m_headers.end()) {
                return def;
            }
            return it->second;
        }

        std::string HttpRequest::getCookie(const std::string& key, const std::string& def) const {
            auto it = m_cookies.find(key);
            if (it == m_cookies.end()) {
                return def;
            }
            return it->second;
        }

        void HttpRequest::setPara(const std::string& key, const std::string& val) {
            m_paras[key] = val;
        }

        void HttpRequest::setHeader(const std::string& key, const std::string& val) {
            m_headers[key] = val;
        }

        void HttpRequest::setCookie(const std::string& key, const std::string& val) {
            m_cookies[key] = val;
        }

        void HttpRequest::delPara(const std::string& key) {
            m_paras.erase(key);
        }

        void HttpRequest::delHeader(const std::string& key) {
            m_headers.erase(key);
        }

        void HttpRequest::delCookie(const std::string& key) {
            m_cookies.erase(key);
        }

        bool HttpRequest::hasPara(const std::string& key, std::string* val /*in-out*/) {
            auto it = m_paras.find(key);
            if (it == m_paras.end()) {
                return false;
            }
            if (val) {
                *val = it->second;
            }
            return true;
        }

        bool HttpRequest::hasHeader(const std::string& key, std::string* val ) {
            auto it = m_headers.find(key);
            if (it == m_headers.end()) {
                return false;
            }
            if (val) {
                *val = it->second;
            }
            return true;
        }

        bool HttpRequest::hasCookie(const std::string& key, std::string* val ) {
            auto it = m_cookies.find(key);
            if (it == m_cookies.end()) {
                return false;
            }
            if (val) {
                *val = it->second;
            }
            return true;
        }

        static const char* s_method_string[] = {
#define XX(num, name, string) #string,
                HTTP_METHOD_MAP(XX)
#undef XX
        };

        const char* HttpMethodToString(const HttpMethod& m) {
            uint32_t idx = (uint32_t)m;
            if (idx >= sizeof(s_method_string) / sizeof(s_method_string[0])) {
                return "<unknown>";
            }
            return s_method_string[idx];
        }

        std::ostream& HttpRequest::dump(std::ostream &os) {
            // GET / HTTP/1.1
            // Host: www.ifeng.com

            os << HttpMethodToString(m_method) << " "
            << m_path
            << (m_query.empty() ? "" : "?")
            << m_query
            << ((m_fragment.empty()) ? "" : "#")
            << m_fragment
            << " HTTP/"
            << (uint32_t)(m_version >> 4)
            << "."
            << (uint32_t)(m_version & 0x0F)
            << "\r\n";

            os << "Connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
            for (const auto& i : m_headers) {
                if (strcasecmp(i.first.c_str(), "connection") == 0) {
                    continue;
                }
                os << i.first << ": " << i.second << "\r\n";
            }

            if (!m_body.empty()) {
                os << "Content-Length: " << m_body.size() << "\r\n";
                os << m_body;
            } else {
                os << "\r\n";
            }
            return os;
        }
    }
}