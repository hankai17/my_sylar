#include "http/http_connection.hh"
#include "log.hh"
#include "iomanager.hh"
#include "address.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test() {
    sylar::IPAddress::ptr addr(sylar::IPv4Address::Create("127.0.0.1", 90));
    sylar::Socket::ptr sock(sylar::Socket::CreateTCP(addr));
    sylar::http::HttpConnection::ptr http_conn(new sylar::http::HttpConnection(sock, false));
    sylar::http::HttpRequest::ptr req(new sylar::http::HttpRequest);
    req->setPath("/path/to/");
    req->setHeader("Host", "127.0.0.1");
    SYLAR_LOG_DEBUG(g_logger) << "Request: " << req->toString();
    sock->connect(addr);

    http_conn->sendRequest(req);
    auto resp = http_conn->recvResponse();
    SYLAR_LOG_DEBUG(g_logger) << "Response: " << resp->toString();
}

void test_chunk() {
    sylar::IPAddress::ptr addr(sylar::IPv4Address::Create("127.0.0.1", 90));
    sylar::Socket::ptr sock(sylar::Socket::CreateTCP(addr));
    sylar::http::HttpConnection::ptr http_conn(new sylar::http::HttpConnection(sock, false));
    sylar::http::HttpRequest::ptr req(new sylar::http::HttpRequest);
    req->setPath("/file.dat");
    req->setHeader("Host", "127.0.0.1");
    //SYLAR_LOG_DEBUG(g_logger) << "Request: " << req->toString();
    sock->connect(addr);

    http_conn->sendRequest(req);
    auto resp = http_conn->recvResponse();
    SYLAR_LOG_DEBUG(g_logger) << "Response: " << resp->toString();
    SYLAR_LOG_DEBUG(g_logger) << "Response cl: " << resp->getBody().size();

}

void test_pool() {
    sylar::http::HttpConnectionPool::ptr pool(new sylar::http::HttpConnectionPool(
            "www.baidu.com", "", 80, 10, 1000 * 30, 5 ));
    sylar::IOManager::GetThis()->addTimer(1000, [pool]() {
        std::map<std::string, std::string> headers;
        headers.insert(std::make_pair("Connection", "Keep-alive"));
        auto r = pool->doGet("/", 300, headers);
        SYLAR_LOG_DEBUG(g_logger) << "pool.use_count: " << pool.use_count()
        << "pool status: " << pool->toString()
        << " Status: " << (r->response ? HttpStatusToString(r->response->getStatus()) : r->toString());
    }, true);
}

int main() {
    sylar::IOManager iom(1, false, "io");
    //iom.schedule(test);
    //iom.schedule(test_chunk);
    iom.schedule(test_pool);
    iom.stop();
    return 0;
}