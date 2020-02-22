#include "log.hh"
#include "http/http_parser.hh"
#include <string>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int main() {
    sylar::http::HttpRequestParser parser;
    std::string req("GET /p HTTP/1.1\r\nHost: www.1.com\r\n\r\ntest");
    int nparsed = parser.execute(const_cast<char*>(req.c_str()), req.size()); // 解析了多个个 不是从0开始的是从1开始计数的
    SYLAR_LOG_DEBUG(g_logger) << "nparsed: " << nparsed << " p[nparsed]: " << int(req[nparsed]) // p[nparsed] 正好指向预解析字串的开头
    << " strlen(req): " << req.size();

    sylar::http::HttpResponseParser parser1;
    //std::string resp("HTTP/1.1 404 Not Found\r\nServer: nginx/1.12.1\r\nDate: Sat, 22 Feb 2020 01:26:24 GMT\r\nContent-Type: text/html\r\nContent-Length: 169\r\nConnection: close\r\n\r\n<html>\r\n<head><title>404 Not Found</title></head>\r\n<body bgcolor=\"white\">\r\n<center><h1>404 Not Found</h1></center>\r\n<hr><center>nginx/1.12.1</center>\r\n</body>\r\n</html>\r\n");
    std::string resp("HTTP/1.1 404 Not Found\r\nServer: nginx\r\n\r\n<html>");
    nparsed = parser1.execute(const_cast<char*>(resp.c_str()), resp.size());
    SYLAR_LOG_DEBUG(g_logger) << "nparsed: " << nparsed << " p[nparsed]: " << int(resp[nparsed])
    << " strlen(resp): " << resp.size();
    // 2020-02-21 17:40:33	49247	0	[DEBUG]	[root]	/root/CLionProjects/my_sylar/tests/http_parser.cc:18	nparsed: 42 p[nparsed]: 104 strlen(resp): 47
    // 解析响应明显有问题
    return 0;
}