#include "ws_server.hh"
#include "my_sylar/log.hh"
#include "my_sylar/hash.hh"

namespace sylar {
    namespace http {
        sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

        WSSession::WSSession(Socket::ptr sock, bool owner)
        : HttpSession(sock, owner) {
        }

        HttpRequest::ptr WSSession::handleShake() {
            HttpRequest::ptr req;
            do {
                req = recvRequest();
                if (!req) {
                    SYLAR_LOG_DEBUG(g_logger) << "invalid http request";
                    break;
                }
                //if (req->getHeader("Upgrade") != "websocket") {
                    //SYLAR_LOG_DEBUG(g_logger) << "request Upgrade not websocket: " << req->getHeader("Upgrade");
                    //break;
                //}
                //req->setHeader("Sec-WebSocket-Key", sylar::base64encode(random_string(16)));

                if (req->GetHeaderAs<int>("Sec-WebSocket-Version") != 13) {
                    SYLAR_LOG_DEBUG(g_logger) << "invalid Sec-WebSocket-Version";
                    break;
                }
                std::string key = req->getHeader("Sec-WebSocket-Key");
                if (key.empty()) {
                    SYLAR_LOG_DEBUG(g_logger) << "Sec-WebSocekt-Key is nil";
                    break;
                }
                std::string v = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
                v = sylar::base64encode(sylar::sha1sum(v));

                auto resp = req->createResponse();
                resp->setStatus(HttpStatus::SWITCHING_PROTOCOLS);
                resp->setWebsocket(true);
                resp->setReason("Web Socket Protocol Handshake");
                resp->setHeader("Upgrade", "Websocket");
                resp->setHeader("Connetion", "Upgrade");
                resp->setHeader("Sec-WebSocket-Accept", v);

                sendResponse(resp);
                SYLAR_LOG_DEBUG(g_logger) << req->toString();
                SYLAR_LOG_DEBUG(g_logger) << resp->toString();
                return req;
            } while (false);
            if (req) {
                SYLAR_LOG_DEBUG(g_logger) << req->toString();
            }
            return nullptr;
        }

        WSFrameMessage::ptr WSSession::recvMessage() {
            return WSRecvMessage(this, false);
        }

        int32_t WSSession::sendMessage(WSFrameMessage::ptr msg, bool fin) {
            return WSSendMessage(this, msg, false, fin);
        }

        int32_t WSSession::sendMessage(const std::string& msg, int32_t opcode, bool fin) {
            return WSSendMessage(this, std::make_shared<WSFrameMessage>(opcode, msg), false, fin);
        }

        int32_t WSSession::ping() {
            return WSPing(this);
        }

        int32_t WSSession::pong() {
            return WSPong(this);
        }

        bool WSSession::handleServerShake() {
            return false;
        }

        bool WSSession::handleClientShake() {
            return false;
        }

        FunctionWSServlet::FunctionWSServlet(callback cb, on_connect_cb connect_cb,
                on_close_cb close_cb) :
                WSServlet("FunctionWSServlet"),
                m_callback(cb),
                m_onConnect(connect_cb),
                m_onClose(close_cb) {
        }

        int32_t FunctionWSServlet::onConnection(HttpRequest::ptr request, WSSession::ptr session) {
            if (m_onConnect) {
                return m_onConnect(request, session);
            }
            return 0;
        }

        int32_t FunctionWSServlet::onClose(HttpRequest::ptr request, WSSession::ptr session) {
            if (m_onClose) {
                return m_onClose(request, session);
            }
            return 0;
        }

        int32_t FunctionWSServlet::handle(HttpRequest::ptr request, WSFrameMessage::ptr msg,
                       WSSession::ptr session) {
            if (m_callback) {
                return m_callback(request, msg, session);
            }
            return 0;
        }

        WSServletDispatch::WSServletDispatch() {
            m_name = "WSServletDispatch";
        }

        void WSServletDispatch::addServlet(const std::string& uri, FunctionWSServlet::callback cb,
                        FunctionWSServlet::on_connect_cb connect_cb,
                        FunctionWSServlet::on_close_cb close_cb) {
            ServletDispatch::addServlet(uri, std::make_shared<FunctionWSServlet>(cb, connect_cb, close_cb));
        }

        void WSServletDispatch::addGlobServlet(const std::string& uri, FunctionWSServlet::callback cb,
                            FunctionWSServlet::on_connect_cb connect_cb,
                            FunctionWSServlet::on_close_cb close_cb) {
            ServletDispatch::addGlobServlet(uri, std::make_shared<FunctionWSServlet>(cb, connect_cb, close_cb));
        }

        WSServlet::ptr WSServletDispatch::getWSServlet(const std::string& uri) {
            auto slt = getMatchedServlet(uri);
            return std::dynamic_pointer_cast<WSServlet>(slt);
        }

        WSServer::WSServer(sylar::IOManager* worker, sylar::IOManager* accept_worker)
        : TcpServer(worker, accept_worker) {
            m_dispatch.reset(new WSServletDispatch);
        }

        void WSServer::handleClient(Socket::ptr client) {
            SYLAR_LOG_DEBUG(g_logger) << "WSServer handleClient";
            WSSession::ptr session(new WSSession(client));
            do {
                HttpRequest::ptr req = session->handleShake();
                if (!req) {
                    SYLAR_LOG_DEBUG(g_logger) << "handleShake err";
                    break;
                }
                WSServlet::ptr servlet = m_dispatch->getWSServlet(req->getPath());
                if (!servlet) {
                    SYLAR_LOG_DEBUG(g_logger) << "no matched WSServlet";
                    break;
                }
                int ret = servlet->onConnection(req, session);
                if (ret) {
                    SYLAR_LOG_DEBUG(g_logger) << "onConnection return: " << ret;
                    break;
                }
                while (true) {
                    auto msg = session->recvMessage();
                    if (!msg) {
                        break;
                    }
                    ret = servlet->handle(req, msg, session);
                    if (ret) {
                        SYLAR_LOG_DEBUG(g_logger) << "handle return: " << ret;
                        break;
                    }
                }
                servlet->onClose(req, session);
            } while (true);
            session->close();
        }

    }
}