#include "protocol.hh"
#include "log.hh"
#include "endian.hh"
#include "config.hh"
#include <sstream>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static sylar::ConfigVar<uint32_t>::ptr g_rock_protocol_max_len =
            sylar::Config::Lookup("rock.protocol.max_len", (uint32_t)(1024 * 1024 * 64), "rock protocol max_len");
    static sylar::ConfigVar<uint32_t>::ptr g_rock_protocol_min_len =
            sylar::Config::Lookup("rock.protocol.min_len", (uint32_t)(1024 * 1), "rock protocol min_len");

    ByteArray::ptr Message::toByteArray() {
        ByteArray::ptr ba(new ByteArray);
        if (serializeToByteArray(ba)) {
            return ba;
        }
        return nullptr;
    }

    Request::Request()
    : m_sn(0),
    m_cmd(0) {
    }

    bool Request::serializeToByteArray(ByteArray::ptr byte_array) {
        byte_array->writeFint8(getType());
        byte_array->writeUint32(m_sn);
        byte_array->writeUint32(m_cmd);
        return true;
    }

    bool Request::parseFromByteArray(ByteArray::ptr byte_array) {
        m_sn = byte_array->readUint32();
        m_cmd = byte_array->readUint32();
        return true;
    }

    Response::Response()
            : m_sn(0),
              m_cmd(0),
              m_result(0),
              m_resultStr() {
    }

    bool Response::serializeToByteArray(ByteArray::ptr byte_array) {
        byte_array->writeFint8(getType());
        byte_array->writeUint32(m_sn);
        byte_array->writeUint32(m_cmd);
        byte_array->writeUint32(m_result);
        byte_array->writeStringVint(m_resultStr);
        return true;
    }

    bool Response::parseFromByteArray(ByteArray::ptr byte_array) {
        m_sn = byte_array->readUint32();
        m_cmd = byte_array->readUint32();
        m_result = byte_array->readUint32();
        m_resultStr = byte_array->readStringVint();
        return true;
    }


    Notify::Notify()
    : m_notify(0) {
    }

    bool Notify::serializeToByteArray(ByteArray::ptr byte_array) {
        byte_array->writeFint8(getType());
        byte_array->writeUint32(m_notify);
        return true;
    }

    bool Notify::parseFromByteArray(ByteArray::ptr byte_array) {
        m_notify = byte_array->readUint32();
        return true;
    }

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



}