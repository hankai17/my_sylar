#include "ws_connection.hh"
#include "log.hh"
#include "address.hh"
#include "hash.hh"
#include "http/http_parser.hh"
#include "buffer.hh"
#include "endian.hh"

#include <string.h>

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
            return;
        }

        WSFrameMessage::ptr WSRecvMessage(Stream* stream, bool client) {
            std::string data;
            int cur_len = 0;
            int first_opcode_type = 0;
            do {
                WSFrameHead frameHead;
                if (stream->readFixSize(&frameHead, sizeof(frameHead)) <= 0) {
                    break;
                }
                SYLAR_LOG_DEBUG(g_logger) << frameHead.toString();
                if (frameHead.opcode == WSFrameHead::PING_FRAME) {
                    SYLAR_LOG_DEBUG(g_logger) << "PING";
                    if (WSPong(stream) <= 0) {
                        break;
                    }
                } else if (frameHead.opcode == WSFrameHead::PONG_FRAME) {
                } else if (frameHead.opcode == WSFrameHead::CONTINUE
                    || frameHead.opcode == WSFrameHead::TEXT_FRAME
                    || frameHead.opcode == WSFrameHead::BIN_FRAME) {
                    if (!client && !frameHead.mask) {
                        SYLAR_LOG_DEBUG(g_logger) << "client frame no mask";
                        break;
                    }
                    uint64_t length;
                    if (frameHead.payload < 125) {
                        length = frameHead.payload;
                    } else if (frameHead.payload == 126) {
                        uint16_t len;
                        if (stream->readFixSize(&len, sizeof(len)) <= 0) {
                            break;
                        }
                        length = sylar::byteswapOnLittleEndian(len);
                    } else if (frameHead.payload == 127) {
                        uint64_t len;
                        if (stream->readFixSize(&len, sizeof(len)) <= 0) {
                            break;
                        }
                        length = sylar::byteswapOnLittleEndian(len);
                    }

                    char mask[4];
                    if (frameHead.mask) {
                        if (stream->readFixSize(mask, sizeof(mask)) <= 0) {
                            break;
                        }
                    }
                    data.resize(cur_len + length); // Just like vector's resize ?
                    if (stream->readFixSize(&data[cur_len], length) <= 0) {
                        break;
                    }
                    if (frameHead.mask) {
                        for (size_t i = 0; i < length; i++) {
                            data[cur_len + i] ^= mask[i % 4];
                        }
                    }
                    cur_len += length;

                    if (!first_opcode_type && frameHead.opcode != WSFrameHead::CONTINUE) {
                        first_opcode_type = frameHead.opcode;
                    }

                    if (frameHead.fin == 1) {
                        SYLAR_LOG_DEBUG(g_logger) << data;
                        return WSFrameMessage::ptr(new WSFrameMessage(first_opcode_type, std::move(data)));
                    }
                } else {
                    SYLAR_LOG_DEBUG(g_logger) << "invalid opcode: " << frameHead.opcode;
                }
            } while (true);
            stream->close();
            return nullptr;
        }

        int32_t WSSendMessage(Stream* stream, WSFrameMessage::ptr msg, bool client, bool fin) {
            do {
                WSFrameHead framehead;
                memset(&framehead, 0, sizeof(framehead));
                framehead.fin = fin;
                framehead.opcode = msg->getOpcode();
                framehead.mask = client;
                uint64_t size = msg->getData().size();
                if (size <= 125) {
                    framehead.payload = size;
                } else if (size < 65535) { // 16bit
                    framehead.payload = 126;
                } else { // 64bit
                    framehead.payload = 127;
                }

                if (stream->writeFixSize(&framehead, sizeof(framehead)) <= 0) { // 立即发送不要攒
                    break;
                }

                if (framehead.payload == 126) {
                    uint16_t psize = size;
                    psize = sylar::byteswapOnLittleEndian(psize);
                    if (stream->writeFixSize(&psize, sizeof(psize)) <= 0) {
                        break;
                    }
                } else if (framehead.payload == 127) {
                    uint64_t psize = size;
                    psize = sylar::byteswapOnLittleEndian(psize);
                    if (stream->writeFixSize(&psize, sizeof(psize)) <= 0) {
                        break;
                    }
                }
                if (client) {
                    char mask[4];
                    uint32_t rand_value = rand();
                    memcpy(mask, &rand_value, sizeof(rand_value));
                    std::string& data = msg->getData();
                    for (size_t i = 0; i < data.size(); i++) {
                        data[i] ^= mask[i % 4];
                    }
                    if (stream->writeFixSize(mask, sizeof(mask)) <= 0) { // WHY here not byteswap ?
                        break;
                    }
                }

                if (stream->writeFixSize(msg->getData().c_str(), msg->getData().size()) <= 0) {
                    break;
                }
                return size + sizeof(framehead);
            } while (0);
            stream->close(); // ?
            return -1;
        }

        int32_t WSPing(Stream* stream) {
            WSFrameHead frameHead;
            memset(&frameHead, 0, sizeof(frameHead));
            frameHead.fin = 1;
            frameHead.opcode = WSFrameHead::PING_FRAME;
            int32_t v;
            if ((v = stream->writeFixSize(&frameHead, sizeof(frameHead))) <= 0) {
                stream->close();
            }
            return v;
        }

        int32_t WSPong(Stream* stream) {
            WSFrameHead frameHead;
            memset(&frameHead, 0, sizeof(frameHead));
            frameHead.fin = 1;
            frameHead.opcode = WSFrameHead::PONG_FRAME;
            int32_t v;
            if ((v = stream->writeFixSize(&frameHead, sizeof(frameHead))) <= 0) {
                stream->close();
            }
            return v;
        }

        std::pair<HttpResult::ptr, WSConnection::ptr> WSConnection::Create(Uri::ptr uri,
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
            if (resp->getStatus() != HttpStatus::SWITCHING_PROTOCOLS) {
                return std::make_pair(std::make_shared<HttpResult>(50, resp, "not upgrade success, not websocket server"), conn);
            }
            return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::OK, resp, "ok"), conn);
        }

        std::pair<HttpResult::ptr, WSConnection::ptr> WSConnection::Create(const std::string& url,
                                                                    uint64_t timeout_ms,
                                                                    const std::map<std::string, std::string>& headers) {
            Uri::ptr uri = Uri::Create(url);
            if (!uri) {
                return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL,
                        nullptr, "invalid url: " + url), nullptr);
            }
            return Create(uri, timeout_ms, headers);
        }

        WSFrameMessage::ptr WSConnection::recvMessage() {
            return WSRecvMessage(this, true);
        }

        int32_t WSConnection::sendMessage(WSFrameMessage::ptr msg, bool fin) {
            return WSSendMessage(this, msg, true, fin);
        }

        int32_t WSConnection::sendMessage(const std::string& msg, int32_t opcode, bool fin) {
            return WSSendMessage(this, std::make_shared<WSFrameMessage>(opcode, msg), true, fin);
        }

        int32_t WSConnection::ping() {
            return WSPing(this);
        }

        int32_t WSConnection::pong() {
            return WSPong(this);
        }

    }
}