#include "ws_connection.hh"
#include "log.hh"
#include "address.hh"

namespace sylar {
    namespace http {
        static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

        std::string WSFrameHead::toString() const {
            std::stringstream ss;
            ss << "fin: " << fin
            << " rsv1: " << rsv1
            << " rsv2: " << rsv2
            << " rsv3: " << rsv3
            << " opcode: " << opcode
            << " mask: " << mask
            << " payload: " << payload;
            return ss.str();
        }

        WSFrameMessage::WSFrameMessage(int opcode, const std::string& data)
        : m_opcode(opcode),
        m_data(data) {
        }

        WSConnection::WSConnection(sylar::Socket::ptr sock, bool owner)
        : HttpConnection(sock, owner) {
        }

        std::pair<HttpResult::ptr, WSConnection::ptr> Create(Uri::ptr uri,
                                                             uint64_t timeout_ms,
                                                             const std::map<std::string, std::string>& headers) {
            Address::ptr addr = uri->createAddress();
            if (!addr) {
                return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_HOST,
                        nullptr, "invalid url: " + uri->getHost()), nullptr);
            }
            Socket::ptr sock = Socket::CreateTCP(addr);
            if (!sock) {
                return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::CREATE_SOCKET_ERROR,
                        nullptr, "create socket failed, errno:" + std::to_string(errno) + " strerrno: " + strerror(errno)),
                                nullptr);
            }
            if (!sock->connect(addr)) {
                return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::CONNECT_FAIL,
                        nullptr, "connect failed: " + addr->toString()), nullptr);
            }
            sock->setRecvTimeout(timeout_ms);
            WSConnection::ptr conn = std::make_shared<WSConnection>(sock);
            HttpRequest::ptr req = std::make_shared<HttpRequest>();
            req->setPath(uri->getPath());
            req->setQuery(uri->getQuery());
            req->setFragment(uri->getFragment());
            req->setMethod(HttpMethod::GET);

            bool has_host = false;
            bool has_conn = false;
            for (const auto& i : headers) {
                if (strcasecmp(i.first.c_str(), "connection") == 0) {
                    has_conn = true;
                } else if (strcasecmp(i.first.c_str(), "host") == 0) {
                    has_host = true;
                }
                req->setHeader(i.first, i.second);
            }
            req->setWebsocket(true);
            if (!has_conn) {
                req->setHeader("connection", "Upgrade");
            }
            req->setHeader("Sec-WebSocket-Version", "13");
            req->setHeader("Upgrade", "websocket");
            req->setHeader("Sec-WebSocket-Key", sylar::base64encode(random_string(16)));
            if (!has_host) {
                req->setHeader("Host", uri->getHost());
            }

            int ret = conn->sendRequest(req);
            if (ret == 0) {
                return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSED_BY_PEER,
                        nullptr, "send request closed by peer: " + addr->toString()), nullptr);
            }
            if (ret < 0) {
                return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::SEND_SOCKET_ERROR,
                        nullptr, "send request socket err, errno: " + std::to_string(errno) + " strerror: " + strerror(errno)),
                                nullptr);
            }
            auto resp = conn->recvResponse();
            if (!resp) {
                return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT,
                         nullptr, "may be timeout & peer closed: " + addr->toString() + " timeout_ms: " + std::to_string(timeout_ms)),
                                      nullptr);
            }

        }

        std::pair<HttpResult::ptr, WSConnection::ptr> Create(const std::string& url,
                                                                    uint64_t timeout_ms,
                                                                    const std::map<std::string, std::string>& headers) {
            Uri::ptr uri = Uri::Create(url);
            if (!uri) {
                return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL,
                        nullptr, "invalid url: " + url), nullptr);
            }
            return Create(uri, timeout_ms, headers);
        }


    }
}