#include "http/http.hh"
#include "log.hh"

#include <string.h>
#include <iostream>

namespace sylar {
    namespace http {

        bool CaseInsensitiveLess::operator() (const std::string& lv,
                const std::string& rv) const {
            return strcasecmp(lv.c_str(), rv.c_str()) < 0; // https://blog.csdn.net/test1280/article/details/100806433 STL code analy
        }

        HttpRequest::HttpRequest(uint8_t version, bool close)
        : m_method(HttpMethod::GET),
        m_version(version),
        m_close(close),
        m_path("/") {
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

        std::shared_ptr<HttpResponse> HttpRequest::createResponse() {
            HttpResponse::ptr resp(new HttpResponse(getVersion(), isClose()));
            return resp;
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

        const char* HttpStatusToString(const HttpStatus& m) {
            switch (m) {
#define XX(num, name, string) \
    case HttpStatus::name :  \
        return #string;
        HTTP_STATUS_MAP(XX);
#undef XX
                default:
                    return "<unknown>";
            }
        }

        HttpMethod CharsToHttpMethod(const char* m) {
#define XX(num, name, string) \
            if (strncmp(#string, m, strlen(#string)) == 0) { \
                return HttpMethod::name; \
            }
    HTTP_METHOD_MAP(XX);
#undef XX
            return HttpMethod::INVALID_METHOD;
        }

        /*
        HttpStatus CharsToHttpStatus(const char* m) {
#define XX(num, name, string) \
            if (strncmp(#string, m, strlen(#string)) == 0) { \
                return HttpStatus::name; \
            }
            HTTP_STATUS_MAP(XX);
#undef XX
            return HttpStatus::
        }
         */

        std::ostream& HttpRequest::dump(std::ostream &os) const { // fragment behind all paras
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

            if (!m_isWebsocket) {
                os << "Connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
            }
            for (const auto& i : m_headers) {
                if ((strcasecmp(i.first.c_str(), "connection") == 0) && !m_isWebsocket) {
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

        std::string HttpRequest::toString() const {
            std::stringstream ss;
            dump(ss);
            return ss.str();
        }

        HttpResponse::HttpResponse(uint8_t version, bool close)
        : m_status(HttpStatus::OK),
        m_version(version),
        m_close(close) {
        }

        std::string HttpResponse::getHeader(const std::string& key, const std::string& def) const {
            auto it = m_headers.find(key);
            if (it == m_headers.end()) {
                return def;
            }
            return it->second;
        }

        void HttpResponse::setHeader(const std::string& key, const std::string& val) {
            //std::cout << "m_headers.size(): " << m_headers.size()
            //<< " key: " << key << "  value: " << val << std::endl;
            m_headers[key] = val; // https://www.cnblogs.com/tianzeng/p/9017148.html
            //m_headers.insert(std::pair<std::string, std::string>(key, val));
        }

        void HttpResponse::delHeader(const std::string& key) {
            m_headers.erase(key);
        }

        std::ostream& HttpResponse::dump(std::ostream& os/*in-return*/) const {
            // HTTP/1.1 200 OK
            os << "HTTP/"
            << (((uint32_t)m_version) >> 4)
            << "."
            << ((uint32_t)m_version & 0x0F)
            << " "
            << (uint32_t)m_status
            << " "
            << ((m_reason.empty()) ? HttpStatusToString(m_status) : m_reason) << "\r\n";

            for (const auto& i : m_headers) {
                if (strcasecmp(i.first.c_str(), "connection") == 0) {
                    continue;
                }
                os << i.first << ": " << i.second << "\r\n";
            }
            os << "Connection: " << ((m_close) ? "close" : "keep-alive") << "\r\n";
            if (!m_body.empty()) {
                os << "Content-Length: " << m_body.size() << "\r\n\r\n";
                //os << m_body;
            } else {
                os << "\r\n";
            }
            return os;
        }

        std::string HttpResponse::toString() const {
            std::stringstream ss;
            dump(ss);
            return ss.str();
        }
    }
}