#ifndef _HTTP_PARSER_HH__
#define _HTTP_PARSER_HH__

#include <memory>
#include "http.hh"
#include "httpclient_parser.hh"
#include "http11_parser.hh"

namespace sylar {
    namespace http {
        class HttpRequestParser {
        public:
            typedef std::shared_ptr<HttpRequestParser> ptr;
            HttpRequestParser();

            size_t execute(char* data, size_t len);
            int isFinished();
            int hasError();

            HttpRequest::ptr getData() const { return m_data; }
            void setError(int v) { m_error = v; }
            uint64_t getContentLength();
            const http_parser& getParser() const { return m_parser; }

            static uint64_t GetHttpRequestBufferSize();
            static uint64_t GetHttpRequestMaxBodySize();

        private:
            http_parser         m_parser;
            HttpRequest::ptr    m_data;
            int                 m_error;
        };

        class HttpResponseParser {
        public:
            typedef std::shared_ptr<HttpResponseParser> ptr;
            HttpResponseParser();

            size_t execute(char* data, size_t len, bool chunk = false);
            int isFinished();
            int hasError();

            HttpResponse::ptr getData() const { return m_data; }
            void setError(int v) { m_error = v; }
            uint64_t getContentLength();
            const httpclient_parser& getParser() const { return m_parser; }

            static uint64_t GetHttpResponseBufferSize();
            static uint64_t GetHttpResponseMaxBodySize();

        private:
            httpclient_parser       m_parser;
            HttpResponse::ptr       m_data;
            int                     m_error;
        };
    }
}

#endif

// 感觉 跟flvProtocol不一样
// flvProtocol调 onParseFlv(rawBuf, len) 接收底层数据 里面再搞个中间层用以解出来一个完整的flvpacket
// 这里的execute就像onParseFlv 接触的是最底层 最原生的rawbuf 然后。。。
// 1 核心问题就是 上层能不能动态的收body? 


