#ifndef _HTTP_PARSER_HH__
#define _HTTP_PARSER_HH__

#include <memory>
#include "http.hh"
#include "httpclient_parser.h"
#include "http11_parser.h"

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

            size_t execute(char* data, size_t len);
            int isFinished();
            int hasError();

            HttpResponse::ptr getData() const { return m_data; }
            void setError(int v) { m_error = v; }
            uint64_t getContentLength();

        private:
            httpclient_parser       m_parser;
            HttpResponse::ptr       m_data;
            int                     m_error;
        };
    }
}

#endif