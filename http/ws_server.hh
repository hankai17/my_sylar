#ifndef __WS_SERVER_HH__
#define __WS_SERVER_HH__

#include <memory>
#include "http/http_server.hh"
#include "http/http.hh"
#include "http/ws_connection.hh"
#include "socket.hh"
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

        private:

        };

        class WSServer : public TcpServer {
        public:
            typedef std::shared_ptr<WSServer> ptr;
            WSServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
                    sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
        private:
        };

    }
}

#endif
