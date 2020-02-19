#ifndef __HTTP_SERVER_HH__
#define __HTTP_SERVER_HH__

#include "socket.hh"
#include "http.hh"
#include "stream.hh"
#include "tcp_server.hh"
#include "iomanager.hh"
#include "thread.hh"
#include <functional>
#include <unordered_map>
#include <vector>

namespace sylar {
    namespace http {
        // session用来收发读写socket上的数据 httprequest httpresonse用来解析
        class HttpSession : public SocketStream {
        public:
            typedef std::shared_ptr<HttpSession> ptr;
            HttpSession(Socket::ptr sock, bool owner = true);
            HttpRequest::ptr recvRequest();
            int sendResponse(HttpResponse::ptr rsp);
        };

        class Servlet {
        public:
            typedef std::shared_ptr<Servlet> ptr;
            Servlet(const std::string& name)
            : m_name(name) {}
            virtual ~Servlet() {}
            virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                    HttpSession::ptr session) = 0;
            const std::string& getName() const { return m_name; }
        private:
            std::string     m_name;
        };

        class FunctionServlet : public Servlet {
        public:
            typedef std::shared_ptr<FunctionServlet> ptr;
            typedef std::function<int32_t (HttpRequest::ptr request, HttpResponse::ptr response,
                    HttpSession::ptr session)> callback;
            FunctionServlet(callback cb);
            virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                                   HttpSession::ptr session) override;
        private:
            callback            m_cb;

        };

        class NotFoundServlet : public Servlet {
        public:
            typedef std::shared_ptr<NotFoundServlet> ptr;
            NotFoundServlet();
            virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                                   HttpSession::ptr session) override;
        };

        class ServletDispatch : public Servlet {
        public:
            typedef std::shared_ptr<ServletDispatch> ptr;
            typedef RWMutex RWMutexType;

            ServletDispatch();
            virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                    HttpSession::ptr session) override;
            void addServlet(const std::string& uri, Servlet::ptr slt);
            void addServlet(const std::string& uri, FunctionServlet::callback cb);
            void addGlobServlet(const std::string& uri, Servlet::ptr slt);
            void addGlobServlet(const std::string& uri, FunctionServlet::callback cb);
            void delServlet(const std::string& uri);
            void delGlobServlet(const std::string& uri);

            Servlet::ptr getDefault() const { return m_default; }
            void setDefault(Servlet::ptr v) { m_default = v; }
            Servlet::ptr getServlet(const std::string& uri);
            Servlet::ptr getGlobalServlet(const std::string& uri);
            Servlet::ptr getMatchedServlet(const std::string& uri);

        private:
            RWMutexType                                         m_mutex;
            std::unordered_map<std::string, Servlet::ptr>       m_datas;
            std::vector<std::pair<std::string, Servlet::ptr> >  m_globs;
            Servlet::ptr                                        m_default;

        };

        class HttpServer : public TcpServer {
        public:
            typedef std::shared_ptr<HttpServer> ptr;
            HttpServer(bool keepalive = false,
                    IOManager* worker = IOManager::GetThis(),
                    IOManager* accept_worker = IOManager::GetThis());
            ServletDispatch::ptr getServletDispatch() const { return m_dispatch; }
            void setServletDispatch(ServletDispatch::ptr v) { m_dispatch = v; }

        protected:
            virtual void handleClient(Socket::ptr client) override;

        private:
            bool                        m_isKeepalive;
            ServletDispatch::ptr        m_dispatch;

        };
    }
}

#endif
