#include "rock/rock.hh"
#include "log.hh"
#include "config.hh"
#include "endian.hh"

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static sylar::ConfigVar<uint32_t>::ptr g_rock_protocol_max_len =
            sylar::Config::Lookup("rock.protocol.max_len", (uint32_t)(1024 * 1024 * 64), "rock protocol max_len");
    static sylar::ConfigVar<uint32_t>::ptr g_rock_protocol_min_len =
            sylar::Config::Lookup("rock.protocol.min_len", (uint32_t)(1024 * 1), "rock protocol min_len");

    bool RockBody::serializeToByteArray(ByteArray::ptr byte_array) {
        byte_array->writeStringVint(m_body);
        return true;
    }

    bool RockBody::parseFromByteArray(ByteArray::ptr byte_array) {
        m_body = byte_array->readStringVint();
        return true;
    }

    std::string RockRequest::toString() const {
        std::stringstream ss;
        ss << "RockRequest sn:" << m_sn
        << " cmd: " << m_cmd
        << " body.len: " << m_body.size();
        return ss.str();
    }

    const std::string& RockRequest::getName() const {
        static const std::string& s_name = "RockRequest";
        return s_name;
    }

    int32_t RockRequest::getType() const {
        return Message::REQUEST;
    }

    bool RockRequest::serializeToByteArray(ByteArray::ptr byte_array) {
        try {
            bool v = true;
            v &= Request::serializeToByteArray(byte_array);
            v &= RockBody::serializeToByteArray(byte_array);
            return v;
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "RockRequest serializeToByteArray err";
        }
        return false;
    }

    bool RockRequest::parseFromByteArray(ByteArray::ptr byte_array) {
        try {
            bool v = true;
            v &= Request::parseFromByteArray(byte_array);
            v &= RockBody::parseFromByteArray(byte_array);
            return v;
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "RockRequest parseFromByteArray err";
        }
        return false;
    }

    std::shared_ptr<RockResponse> RockRequest::createResponse() {
        RockResponse::ptr resp(new RockResponse);
        resp->setSn(m_sn);
        resp->setCmd(m_cmd);
        return resp;
    }

    std::string RockResponse::toString() const {
        std::stringstream ss;
        ss << "RockResponse sn:" << m_sn
           << " cmd: " << m_cmd
           << " result: " << m_result
           << " resultStr: " << m_resultStr
           << " body.len: " << m_body.size();
        return ss.str();
    }

    const std::string& RockResponse::getName() const {
        static const std::string& s_name = "RockResponse";
        return s_name;
    }

    int32_t RockResponse::getType() const {
        return Message::RESPONSE;
    }

    bool RockResponse::serializeToByteArray(ByteArray::ptr byte_array) {
        try {
            bool v = true;
            v &= Response::serializeToByteArray(byte_array);
            v &= RockBody::serializeToByteArray(byte_array);
            return v;
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "RockResponse serializeToByteArray err";
        }
        return false;
    }

    bool RockResponse::parseFromByteArray(ByteArray::ptr byte_array) {
        try {
            bool v = true;
            v &= Response::parseFromByteArray(byte_array);
            v &= RockBody::parseFromByteArray(byte_array);
            return v;
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "RockResponse parseFromByteArray err";
        }
        return false;
    }

    std::string RockNotify::toString() const {
        std::stringstream ss;
        ss << "RockNotify notify:" << m_notify
           << " body.len: " << m_body.size();
        return ss.str();
    }

    const std::string& RockNotify::getName() const {
        static const std::string& s_name = "RockNotify";
        return s_name;
    }

    int32_t RockNotify::getType() const {
        return Message::NOTIFY;
    }

    bool RockNotify::serializeToByteArray(ByteArray::ptr byte_array) {
        try {
            bool v = true;
            v &= Notify::serializeToByteArray(byte_array);
            v &= RockBody::serializeToByteArray(byte_array);
            return v;
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "RockNotify serializeToByteArray err";
        }
        return false;
    }

    bool RockNotify::parseFromByteArray(ByteArray::ptr byte_array) {
        try {
            bool v = true;
            v &= Notify::parseFromByteArray(byte_array);
            v &= RockBody::parseFromByteArray(byte_array);
            return v;
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "RockNotify parseFromByteArray err";
        }
        return false;
    }

    static const uint8_t s_rock_magic[2] = {0x95, 0x27};

    RockMsgHeader::RockMsgHeader()
    : magic{0x95, 0x27},
    version(1),
    flag(0),
    length(0) {
    }


    Message::ptr RockMessageDecoder::parseFrom(Stream::ptr stream) {
        try {
            RockMsgHeader header;
            if (stream->readFixSize(&header, sizeof(header)) < 0) {
                SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder decode head error";
                return nullptr;
            }
            if (memcmp(header.magic, s_rock_magic, sizeof(s_rock_magic))) { // ???
                SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder decode head error";
                return nullptr;
            }
            if (header.version != 0x01) {
                SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder header.version != 0x01";
                return nullptr;
            }
            header.length = sylar::byteswapOnLittleEndian(header.length);
            if ((uint32_t)header.length >= g_rock_protocol_max_len->getValue()) {
                SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder header.length: "
                << header.length << " > " << g_rock_protocol_max_len;
                return nullptr;
            }
            sylar::ByteArray::ptr ba(new ByteArray);
            if (stream->readFixSize(ba, header.length) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder read body failed: " << header.length;
                return nullptr;
            }

            if (header.flag & 0x01) { // TODO gzip
            }
            uint8_t type = ba->readFUint8();
            Message::ptr msg;
            switch (type) {
                case Message::REQUEST: {
                    msg.reset(new RockRequest);
                    break;
                }
                case Message::RESPONSE: {
                    msg.reset(new RockResponse);
                    break;
                }
                case Message::NOTIFY: {
                    msg.reset(new RockNotify);
                    break;
                }
                default: {
                    SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder invalid type: " << type;
                    return nullptr;
                }
            }
            if (!msg->parseFromByteArray(ba)) {
                SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder parseFromByteArray failed, type: "
                << type;
                return nullptr;
            }
            return msg;
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder except...";
        }
        return nullptr;
    }

    int32_t RockMessageDecoder::serializeTo(Stream::ptr stream, Message::ptr msg) {
        RockMsgHeader header;
        auto ba = msg->toByteArray();
        ba->setPosition(0);
        header.length = ba->getSize();
        if ((uint32_t)header.length >= g_rock_protocol_min_len->getValue()) {
            {
                //TODO gzip
            }
            //header.flag =
            header.length = ba->getSize();
        }
        header.length = sylar::byteswapOnLittleEndian(header.length);
        if (stream->writeFixSize(&header, sizeof(header)) <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder serializeTo write header failed";
            return -3;
        }
        if (stream->writeFixSize(ba, -1) <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "RockMessageDecoder serializeTo write body failed";
            return -4;
        }
        return sizeof(header) + ba->getSize();
    }

    RockStream::RockStream(Socket::ptr sock)
    : AsyncSocketStream(sock, true),
    m_decoder(new RockMessageDecoder) {
    }

    int32_t RockStream::sendMessage(Message::ptr msg) {
        RockSendCtx::ptr ctx(new RockSendCtx);
        ctx->msg = msg;
        enqueue(ctx);
        return 1;
    }

    RockResult::ptr RockStream::request(RockRequest::ptr request, uint32_t timeout_ms) {
        RockCtx::ptr ctx(new RockCtx);
        ctx->request = request;
        ctx->sn = request->getSn();
        ctx->timeout = timeout_ms;
        ctx->scheduler = sylar::Scheduler::GetThis();
        ctx->fiber = sylar::Fiber::GetThis();
        addCtx(ctx);
        enqueue(ctx);
        sylar::Fiber::YeildToHold();
        return std::make_shared<RockResult>(ctx->result, ctx->response);
    }

    bool RockStream::RockSendCtx::doSend(AsyncSocketStream::ptr stream) {
        return std::dynamic_pointer_cast<RockStream>(stream)->m_decoder->serializeTo(stream, msg) > 0;
    }

    bool RockStream::RockCtx::doSend(AsyncSocketStream::ptr stream) {
        return std::dynamic_pointer_cast<RockStream>(stream)->m_decoder->serializeTo(stream, request) > 0;
    }

    AsyncSocketStream::Ctx::ptr RockStream::doRecv() {
        auto msg = m_decoder->parseFrom(shared_from_this());
        if (!msg) {
            innerClose();
            return nullptr;
        }
        int type = msg->getType();
        if (type == Message::RESPONSE) {
            auto resp = std::dynamic_pointer_cast<RockResponse>(msg);
            if (!resp) {
                SYLAR_LOG_WARN(g_logger) << "RockStream::doRecv not RockResponse: "
                << msg->toString();
                return nullptr;
            }
            RockCtx::ptr ctx = getAndDelCtx<RockCtx>(resp->getSn());
            ctx->response = resp;
            return ctx;
        } else if (type == Message::REQUEST) {
            auto req = std::dynamic_pointer_cast<RockRequest>(msg);
            if (!req) {
                SYLAR_LOG_WARN(g_logger) << "RockStream::doRecv not RockRequest: "
                                         << msg->toString();
                return nullptr;
            }
            if (m_requestHandler) {
                m_iomanager->schedule(std::bind(&RockStream::handleRequest,
                        std::dynamic_pointer_cast<RockStream>(shared_from_this()), req));
            }
        } else if (type == Message::NOTIFY) {
            auto noti = std::dynamic_pointer_cast<RockNotify>(msg);
            if (!noti) {
                SYLAR_LOG_WARN(g_logger) << "RockStream::doRecv not RockNotify: "
                                         << msg->toString();
                return nullptr;
            }
            if (m_notifyHandler) {
                m_iomanager->schedule(std::bind(&RockStream::handleNotify,
                        std::dynamic_pointer_cast<RockStream>(shared_from_this()), noti));
            }
        } else {
            SYLAR_LOG_WARN(g_logger) << "RockStream::doRecv unknow type: " << type
            << " msg: " << msg->toString();
        }
        return nullptr;
    }

    void RockStream::handleRequest(RockRequest::ptr req) {
        sylar::RockResponse::ptr resp = req->createResponse();
        if (!m_requestHandler(req, resp,
                std::dynamic_pointer_cast<RockStream>(shared_from_this()))) {
            innerClose();
        }
    }

    void RockStream::handleNotify(RockNotify::ptr noti) {
        if (!m_notifyHandler(noti,
                std::dynamic_pointer_cast<RockStream>(shared_from_this()))) {
            innerClose();
        }
    }

    RockSession::RockSession(Socket::ptr sock)
    : RockStream(sock) {
        m_autoConnect = false;
    };

    RockConnection::RockConnection()
    : RockStream(nullptr) {
        m_autoConnect = true;
    }

    bool RockConnection::connect(sylar::Address::ptr addr) {
        m_socket = sylar::Socket::CreateTCP(addr);
        return m_socket->connect(addr);
    }
}