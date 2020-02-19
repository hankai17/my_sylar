#include "log.hh"
#include "http/http_server.hh"
#include "iomanager.hh"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run() {
    sylar::http::HttpServer::ptr server(new sylar::http::HttpServer);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("127.0.0.1", 9527);
    //server->bind(addr);
    while (!server->bind(addr)) {
        sleep(2);
    }
    auto sd = server->getServletDispatch();
    sd->addServlet("/my_sylar/xx", [](sylar::http::HttpRequest::ptr req,
            sylar::http::HttpResponse::ptr resp,
            sylar::http::HttpSession::ptr session) {
        resp->setBody(req->toString());
        return 0;
    });

    sd->addGlobServlet("/my_sylar/*", [](sylar::http::HttpRequest::ptr req,
            sylar::http::HttpResponse::ptr resp,
            sylar::http::HttpSession::ptr session ) {
        resp->setBody("Glob:\r\n" + req->toString());
        return 0;
    });
    server->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(run);
    iom.stop();
    return 0;
}