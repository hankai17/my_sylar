#include "http_server.hh"
#include "http_parser.hh"
#include "log.hh"
#include <fnmatch.h>

namespace sylar {
    namespace http {
        static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
        HttpSession::HttpSession(Socket::ptr sock, bool owner)
        : SocketStream(sock, owner) {
        }

        HttpRequest::ptr HttpSession::recvRequest() {
            HttpRequestParser::ptr parser(new HttpRequestParser);
            uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
            std::shared_ptr<char> buffer (new char[buff_size], [](char* ptr) { // 当这个函数调用结束后 delete堆上的空间
                delete[] ptr;
            });
            char* data = buffer.get();
            int offset = 0;
            do {
                int len = read(data + offset, buff_size - offset);
                if (len <= 0) {
                    return nullptr;
                }
                len += offset;
                size_t nparsed = parser->execute(data, len);
                if (parser->hasError()) {
                    return nullptr;
                }
                offset = len - nparsed;
                if (offset == (int)buff_size) {
                    return nullptr;
                }
                if (parser->isFinished()) {
                    break;
                }
            } while (true);
            int64_t length = parser->getContentLength();
            if (length > 0) {
                std::string body;
                body.reserve(length);
                if (length >= offset) {
                    body.append(data, offset);
                } else {
                    body.append(data, length);
                }
                length -= offset;
                if (length > 0) {
                    if (readFixSize(&body[body.size()], length) <= 0) {
                        return nullptr;
                    }
                }
                parser->getData()->setBody(body);
            }
            return parser->getData();
        }

        int HttpSession::sendResponse(HttpResponse::ptr rsp) {
            std::stringstream ss;
            ss << rsp->toString();
            std::string data = ss.str();
            return writeFixSize(data.c_str(), data.size());
        }

        //////////////////////////////////////////////////////////////////////////////////////////

        FunctionServlet::FunctionServlet(callback cb)
        : Servlet("FunctionServlet"),
        m_cb(cb) {
        }

        int32_t FunctionServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                               HttpSession::ptr session) {
            return m_cb(request, response, session);
        }

        NotFoundServlet::NotFoundServlet()
        : Servlet("NotFoundServlet") {
        }

        int32_t NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                                        HttpSession::ptr session) {
            static const std::string& RSP_BODY = "<html><head><title>404 Not Found"
                                                 "</title></head><body><center><h1>404 Not Found</h1></center>"
                                                 "<hr><center>sylar/1.0.0</center></body></html>";
            response->setStatus(HttpStatus::NOT_FOUND);
            response->setHeader("Server", "my_sylar/1.0.0");
            response->setHeader("Content-Type", "text/html");
            response->setBody(RSP_BODY);
            return 0;
        }

        ServletDispatch::ServletDispatch()
        : Servlet("ServletDispatch") {
            m_default.reset(new NotFoundServlet());
        }

        int32_t ServletDispatch::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                               HttpSession::ptr session) {
            auto slt = getMatchedServlet(request->getPath());
            if (slt) {
                slt->handle(request, response, session);
            }
            return 0;
        }

        void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr slt) {
            RWMutexType::WriteLock lock(m_mutex);
            m_datas[uri] = slt;
        }

        void ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback cb) {
            RWMutexType::WriteLock lock(m_mutex);
            m_datas[uri].reset(new FunctionServlet(cb));
        }

        void ServletDispatch::addGlobServlet(const std::string& uri, Servlet::ptr slt) {
            RWMutexType::WriteLock lock(m_mutex);
            for (auto it = m_globs.begin(); it != m_globs.end(); it++) {
                if (it->first == uri) {
                    m_globs.erase(it);
                    break;
                }
            }
            m_globs.push_back(std::make_pair(uri, slt));
        }

        void ServletDispatch::addGlobServlet(const std::string& uri, FunctionServlet::callback cb) {
            return addGlobServlet(uri, FunctionServlet::ptr(new FunctionServlet(cb)));
        }

        void ServletDispatch::delServlet(const std::string& uri) {
            RWMutexType::WriteLock lock(m_mutex);
            m_datas.erase(uri);
        }

        void ServletDispatch::delGlobServlet(const std::string& uri) {
            RWMutexType::WriteLock lock(m_mutex);
            for (auto it = m_globs.begin(); it != m_globs.end(); it++) {
                if (it->first == uri) {
                    m_globs.erase(it);
                    break;
                }
            }
        }

        Servlet::ptr ServletDispatch::getServlet(const std::string& uri) {
            RWMutexType::ReadLock lock(m_mutex);
            auto it = m_datas.find(uri);
            if (it == m_datas.end()) {
                return nullptr;
            }
            return it->second;
        }

        Servlet::ptr ServletDispatch::getGlobalServlet(const std::string& uri) {
            RWMutexType::ReadLock lock(m_mutex);
            for (auto it = m_globs.begin(); it != m_globs.end(); it++) {
                if (it->first == uri) {
                    return it->second;
                }
            }
            return nullptr;
        }

        Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri) {
            RWMutexType::ReadLock lock(m_mutex);
            auto mit = m_datas.find(uri);
            if (mit != m_datas.end()) {
                return mit->second;
            }
            for (auto it = m_globs.begin(); it != m_globs.end(); it++) {
                if (!fnmatch(it->first.c_str(), uri.c_str(), 0)) {
                    return it->second;
                }
            }
            return m_default;
        }

        ///////////////////////////////////////////////////////////////////////

        HttpServer::HttpServer(bool keepalive,
                IOManager* worker, IOManager* accept_worker)
                   :TcpServer(worker, accept_worker),
                    m_isKeepalive(keepalive) {
            m_dispatch.reset(new ServletDispatch);
        }

        void HttpServer::handleClient(Socket::ptr client) {
            HttpSession::ptr session(new HttpSession(client));
            do {
                auto req = session->recvRequest();
                if (!req) {
                    SYLAR_LOG_WARN(g_logger) << "recv http request failed, errno: "
                    << errno << " strerror: " << strerror(errno)
                    << " client: " << client->toString();
                    break;
                }
                HttpResponse::ptr resp(new HttpResponse(req->getVersion(),
                        req->isClose() || !m_isKeepalive));
                resp->setHeader("Server", getName());
                m_dispatch->handle(req, resp, session);
                session->sendResponse(resp);

            } while (m_isKeepalive);
            session->close();
        }

    }
}