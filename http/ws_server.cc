#include "ws_server.hh"
#include "log.hh"
#include "hash.hh"

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

    }
}