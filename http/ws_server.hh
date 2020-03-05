#ifndef __WS_SERVER_HH__
#define __WS_SERVER_HH__

#include <memory>
#include <functional>
#include "http/http_server.hh"
#include "http/http.hh"
#include "http/ws_connection.hh"
#include "socket.hh"
#include "thread.hh"
#include "tcp_server.hh"

namespace sylar {
    namespace http {
        class WSSession : public HttpSession {
        public:
            typedef std::shared_ptr<WSSession> ptr;
            WSSession(Socket::ptr sock, bool owner = true);
            HttpRequest::ptr handleShake();
            WSFrameMessage::ptr recvMessage();
            int32_t sendMessage(WSFrameMessage::ptr msg, bool fin = true);
            int32_t sendMessage(const std::string& msg, int32_t opcode = WSFrameHead::TEXT_FRAME, bool fin = true);
            int32_t ping();
            int32_t pong();
        private:
            bool handleServerShake();
            bool handleClientShake();
        };

        class WSServlet : public Servlet {
        public:
            typedef std::shared_ptr<WSServlet> ptr;
            WSServlet(const std::string& name)
            : Servlet(name) {}
            virtual ~WSServlet() {}

            virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                    HttpSession::ptr session) override {
                return 0;
            }
            virtual int32_t onConnection(HttpRequest::ptr request, WSSession::ptr session) = 0;
            virtual int32_t onClose(HttpRequest::ptr request, WSSession::ptr session) = 0;
            virtual int32_t handle(HttpRequest::ptr request, WSFrameMessage::ptr msg,
                    WSSession::ptr session) = 0;
            const std::string& getName() const { return m_name; }
        protected:
            std::string     m_name;

        };

        class FunctionWSServlet : public WSServlet {
        public:
            typedef std::shared_ptr<FunctionWSServlet> ptr;
            typedef std::function<int32_t (HttpRequest::ptr request,
                    WSSession::ptr session)> on_connect_cb;
            typedef std::function<int32_t (HttpRequest::ptr request,
                    WSSession::ptr session)> on_close_cb;
            typedef std::function<int32_t (HttpRequest::ptr request, WSFrameMessage::ptr msg,
                    WSSession::ptr session)> callback;

            FunctionWSServlet(callback cb, on_connect_cb connect_cb = nullptr,
                    on_close_cb close_cb = nullptr);

            int32_t onConnection(HttpRequest::ptr request, WSSession::ptr session) override;
            int32_t onClose(HttpRequest::ptr request, WSSession::ptr session) override;
            int32_t handle(HttpRequest::ptr request, WSFrameMessage::ptr msg,
                           WSSession::ptr session) override;
        protected:
            callback        m_callback;
            on_connect_cb   m_onConnect;
            on_close_cb     m_onClose;
        };

        class WSServletDispatch : public ServletDispatch {
        public:
            typedef std::shared_ptr<WSServletDispatch> ptr;
            typedef RWMutex RWMutexType;

            WSServletDispatch();
            void addServlet(const std::string& uri, FunctionWSServlet::callback cb,
                    FunctionWSServlet::on_connect_cb connect_cb = nullptr,
                    FunctionWSServlet::on_close_cb close_cb = nullptr);
            void addGlobServlet(const std::string& uri, FunctionWSServlet::callback cb,
                            FunctionWSServlet::on_connect_cb connect_cb = nullptr,
                            FunctionWSServlet::on_close_cb close_cb = nullptr);
            WSServlet::ptr getWSServlet(const std::string& uri);
        private:
            std::string         m_name;
        };

        class WSServer : public TcpServer {
        public:
            typedef std::shared_ptr<WSServer> ptr;
            WSServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
                    sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
            WSServletDispatch::ptr getWSServletDispatch() const { return m_dispatch; }
            void setWSServletDispatch(WSServletDispatch::ptr v) { m_dispatch = v; }
        protected:
            virtual void handleClient(Socket::ptr client) override;
        private:
            WSServletDispatch::ptr      m_dispatch;
        };

    }
}

#endif
