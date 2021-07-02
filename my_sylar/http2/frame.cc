#include <sstream>
#include "my_sylar/http2/frame.hh"
#include "my_sylar/log.hh"

namespace sylar {
namespace http2 {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

std::string FrameHeader::toString() const {
    std::stringstream ss;
    ss << "[FrameHeader length=" << length
       << " type=" << FrameTypeToString((FrameType)type)
       << " flags=" << FrameFlagToString(type, flags)
       << " r=" << (int)r
       << " identifier=" << identifier
       << "]";
    return ss.str();
}

bool FrameHeader::writeTo(ByteArray::ptr ba) {
    try {
        ba->writeFuint32(len_type);
        ba->writeFuint8(flags);
        ba->writeFuint32(r_id);
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write FrameHeader fail, " << toString();
    }
    return false;
}

bool FrameHeader::readFrom(ByteArray::ptr ba) {
    try {
        len_type = ba->readFUint32();
        flags = ba->readFUint8();
        r_id = ba->readFUint32();
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read FrameHeader fail, " << toString();
    }
    return false;
}


std::string DataFrame::toString() const {
    std::stringstream ss;
    ss << "[DataFrame";
    if(pad) {
        ss << " pad=" << (uint32_t)pad;
    }
    ss << " data.size=" << data.size();
    ss << "]";
    return ss.str();
}

bool DataFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        if(header.flags & (uint8_t)FrameFlagData::PADDED) {
            ba->writeFuint8(pad); // ?
            ba->write(data.c_str(), data.size());
            ba->write(padding.c_str(), padding.size());
        } else {
            ba->write(data.c_str(), data.size());
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write DataFrame fail, " << toString();
    }
    return false;
}

bool DataFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        if(header.flags & (uint8_t)FrameFlagData::PADDED) {
            pad = ba->readFUint8();
            data.resize(header.length - pad - 1);
            ba->read(&data[0], data.size());
            padding.resize(pad);
            ba->read(&padding[0], pad);
        } else {
            data.resize(header.length);
            ba->read(&data[0], data.size());
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read DataFrame fail, " << toString();
    }
    return false;
}

std::string PriorityFrame::toString() const {
    std::stringstream ss;
    ss << "[PriorityFrame exclusive=" << exclusive
       << " stream_dep=" << stream_dep
       << " weight=" << weight << "]";
    return ss.str();
}

bool PriorityFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        ba->writeFuint32(e_stream_dep);
        ba->writeFuint8(weight);
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write PriorityFrame fail, " << toString();
    }
    return false;
}

bool PriorityFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        e_stream_dep = ba->readFUint32();
        weight = ba->readFUint8();
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read PriorityFrame fail, " << toString();
    }
    return false;
}

std::string HeadersFrame::toString() const {
    std::stringstream ss;
    ss << "[HeadersFrame";
    if (pad) {
        ss << " pad=" << (uint32_t)pad;
    }
    //ss << " fields.size=" << fields.size();
    ss << "]";
    return ss.str();
}

bool HeadersFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        if (header.flags & (uint8_t)FrameFlagHeaders::PADDED) {
            ba->writeFuint8(pad);
        }
        if (header.flags & (uint8_t)FrameFlagHeaders::PRIORITY) {
            priority.writeTo(ba, header);
        }
#if 0
        for (auto& i : fields) {
            HPack::Pack(&i, ba);
        }
#endif
        ba->write(data.c_str(), data.size());
        if (header.flags & (uint8_t)FrameFlagHeaders::PADDED) {
            ba->write(padding.c_str(), padding.size());
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write HeadersFrame fail, " << toString();
    }
    return false;
}

bool HeadersFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        int len = header.length;
        if (header.flags & (uint8_t)FrameFlagHeaders::PADDED) {
            pad = ba->readFUint8();
            len -= 1 + pad;
        }
        if (header.flags & (uint8_t)FrameFlagHeaders::PRIORITY) {
            priority.readFrom(ba, header);
            len -= 5;
        }
        data.resize(len);
        ba->read(&data[0], data.size());
        if (header.flags & (uint8_t)FrameFlagHeaders::PADDED) {
            padding.resize(pad);
            ba->read(&padding[0], padding.size());
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read HeadersFrame fail, " << toString();
    }
    return false;
}

std::string RstFrame::toString() const {
    std::stringstream ss;
    ss << "[RstFrame error_code=" << error_code << "]";
    return ss.str();
}

bool RstFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        ba->writeFuint32(error_code);
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write RstFrame fail, " << toString();
    }
    return false;
}

bool RstFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        error_code = ba->readFUint32();
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read RstFrame fail, " << toString();
    }
    return false; 
}

static std::vector<std::string> s_settings_string = {
        "",
        "HEADER_TABLE_SIZE",
        "ENABLE_PUSH",
        "MAX_CONCURRENT_STREAMS",
        "INITIAL_WINDOW_SIZE",
        "MAX_FRAME_SIZE",
        "MAX_HEADER_LIST_SIZE"
};

std::string SettingsFrame::SettingsToString(Settings s) {
    uint32_t idx = (uint32_t)s;
    if(idx <= 0 || idx > 6) {
        return "UNKONW(" + std::to_string(idx) + ")";
    }
    return s_settings_string[idx];
}

std::string SettingsItem::toString() const {
    std::stringstream ss;
    ss << "[SettingsFrame identifier="
       << SettingsFrame::SettingsToString((SettingsFrame::Settings)identifier)
       << " value=" << value << "]";
    return ss.str();
}

bool SettingsItem::writeTo(ByteArray::ptr ba) {
    ba->writeFuint16(identifier);
    ba->writeFuint32(value);
    return true;

}

bool SettingsItem::readFrom(ByteArray::ptr ba) {
    identifier = ba->readFUint16();
    value = ba->readFUint32();
    return true;
}

bool SettingsFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        for(auto& i : items) {
            i.writeTo(ba);
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write SettingsFrame fail, " << toString();
    }
    return false;
}

bool SettingsFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        uint32_t size = header.length / sizeof(SettingsItem);
        items.resize(size);
        for(uint32_t i = 0; i < size; ++i) {
            items[i].readFrom(ba);
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read SettingsFrame fail, " << toString();
    }
    return false;
}

std::string SettingsFrame::toString() const {
    std::stringstream ss;
    ss << "[SettingsFrame size=" << items.size() << " items=[";
    for(auto& i : items) {
        ss << i.toString();
    }
    ss << "]]";
    return ss.str();
}


std::string PushPromisedFrame::toString() const {
    std::stringstream ss;
    ss << "[PushPromisedFrame";
    if(pad) {
        ss << " pad=" << (uint32_t)pad;
    }
    ss << " data.size=" << data.size();
    ss << "]";
    return ss.str();
}

bool PushPromisedFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        if(header.flags & (uint8_t)FrameFlagPromise::PADDED) {
            ba->writeFuint8(pad);
            ba->writeFuint32(r_stream_id);
            ba->write(data.c_str(), data.size());
            ba->write(padding.c_str(), padding.size());
        } else {
            ba->writeFuint32(r_stream_id);
            ba->write(data.c_str(), data.size());
        }
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write PushPromisedFrame fail, " << toString();
    }
    return false;
}

bool PushPromisedFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        if(header.flags & (uint8_t)FrameFlagPromise::PADDED) {
            pad = ba->readFUint8();
            r_stream_id = ba->readFUint32();
            data.resize(header.length - 5 - pad);
            ba->read(&data[0], data.size());
            padding.resize(pad);
            ba->read(&padding[0], padding.size());
        } else {
            r_stream_id = ba->readFUint32();
            data.resize(header.length - 4);
            ba->read(&data[0], data.size());
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read PushPromisedFrame fail, " << toString();
    }
    return false;
}


std::string PingFrame::toString() const {
    std::stringstream ss;
    ss << "[PingFrame uint64=" << uint64 << "]";
    return ss.str();
}

bool PingFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        ba->write(data, 8);
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write PingFrame fail, " << toString();
    }
    return false;
}

bool PingFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        ba->read(data, 8);
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read PingFrame fail, " << toString();
    }
    return false;
}

std::string GoAwayFrame::toString() const {
    std::stringstream ss;
    ss << "[GoAwayFrame r=" << r
       << " last_stream_id=" << last_stream_id
       << " error_code=" << error_code
       << " debug.size=" << data.size()
       << "]";
    return ss.str();
}

bool GoAwayFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        ba->writeFuint32(r_last_stream_id);
        ba->writeFuint32(error_code);
        if(!data.empty()) {
            ba->write(data.c_str(), data.size());
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write GoAwayFrame fail, " << toString();
    }
    return false;
}

bool GoAwayFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        r_last_stream_id = ba->readFUint32();
        error_code = ba->readFUint32();
        if(header.length > 8) {
            data.resize(header.length - 8);
            ba->read(&data[0], data.size());
        }
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read GoAwayFrame fail, " << toString();
    }
    return false;
}

std::string WindowUpdateFrame::toString() const {
    std::stringstream ss;
    ss << "[WindowUpdateFrame r=" << r
       << " increment=" << increment
       << "]";
    return ss.str();
}

bool WindowUpdateFrame::writeTo(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        ba->writeFuint32(r_increment);
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "write WindowUpdateFrame fail, " << toString();
    }
    return false;
}

bool WindowUpdateFrame::readFrom(ByteArray::ptr ba, const FrameHeader& header) {
    try {
        r_increment = ba->readFUint32();
        return true;
    } catch(...) {
        SYLAR_LOG_WARN(g_logger) << "read WindowUpdateFrame fail, " << toString();
    }
    return false;
}

static const std::vector<std::string> s_frame_types = {
    "DATA",
    "HEADERS",
    "PRIORITY",
    "RST_STREAM",
    "SETTINGS",
    "PUSH_PROMISE",
    "PING",
    "GOAWAY",
    "WINDOW_UPDATE",
    "CONTINUATION",
};

std::string FrameTypeToString(FrameType type) {
    uint8_t v = (uint8_t)type;
    if(v > 9) {
        return "UNKONW(" + std::to_string(v) + ")";
    }
    return s_frame_types[v];
}

#define XX(ff, str) \
    std::string rt; \
    if((uint8_t)flag & (uint8_t)ff) { \
        rt = str; \
    }

#define XX_IF(ff, str) \
    if((uint8_t)flag & (uint8_t)ff) { \
        if(!rt.empty()) { \
            rt += "|"; \
        } \
        rt += str; \
    }

#define XX_END() \
    if(rt.empty()) { \
        if((uint8_t)flag) { \
            rt = "UNKONW(" + std::to_string((uint32_t)flag) + ")"; \
        } else { \
            rt = "0"; \
        } \
    } \
    return rt;


std::string FrameFlagDataToString(FrameFlagData flag) {
    XX(FrameFlagData::END_STREAM, "END_STREAM");
    XX_IF(FrameFlagData::PADDED, "PADDED");
    XX_END();
}

std::string FrameFlagHeadersToString(FrameFlagHeaders flag) {
    XX(FrameFlagHeaders::END_STREAM, "END_STREAM");
    XX_IF(FrameFlagHeaders::END_HEADERS, "END_HEADERS");
    XX_IF(FrameFlagHeaders::PADDED, "PADDED");
    XX_IF(FrameFlagHeaders::PRIORITY, "PRIORITY");
    XX_END();
}

std::string FrameFlagSettingsToString(FrameFlagSettings flag) {
    XX(FrameFlagSettings::ACK, "ACK");
    XX_END();
}
std::string FrameFlagPingToString(FrameFlagPing flag) {
    XX(FrameFlagPing::ACK, "ACK");
    XX_END();
}
std::string FrameFlagContinuationToString(FrameFlagContinuation flag) {
    XX(FrameFlagContinuation::END_HEADERS, "END_HEADERS");
    XX_END();
}
std::string FrameFlagPromiseToString(FrameFlagPromise flag) {
    XX(FrameFlagPromise::END_HEADERS, "END_HEADERS");
    XX_IF(FrameFlagPromise::PADDED, "PADDED");
    XX_END();
}

std::string FrameFlagToString(uint8_t type, uint8_t flag) {
    switch((FrameType)type) {
        case FrameType::DATA:
            return FrameFlagDataToString((FrameFlagData)flag);
        case FrameType::HEADERS:
            return FrameFlagHeadersToString((FrameFlagHeaders)flag);
        case FrameType::SETTINGS:
            return FrameFlagSettingsToString((FrameFlagSettings)flag);
        case FrameType::PING:
            return FrameFlagPingToString((FrameFlagPing)flag);
        case FrameType::CONTINUATION:
            return FrameFlagContinuationToString((FrameFlagContinuation)flag);
        default:
            return "UNKONW(" + std::to_string((uint32_t)flag) + ")";
    }
}

std::string FrameRToString(FrameR r) {
    if(r == FrameR::SET) {
        return "SET";
    } else {
        return "UNSET";
    }
}

std::string Frame::toString() const {
    std::stringstream ss;
    ss << header.toString();
    if (data) {
        ss << ", ";
        ss << data->toString();
    }
    return ss.str();
}

Frame::ptr FrameCodec::parseFrom(Stream::ptr stream) {
    try {
        Frame::ptr frame = std::make_shared<Frame>();
        ByteArray::ptr ba(new ByteArray);
        int ret = stream->readFixSize(ba, FrameHeader::SIZE);
        if (ret <= 0) {
            SYLAR_LOG_INFO(g_logger) << "recv frame header failed, ret=" << ret << ", " << strerror(errno);
            return nullptr;
        }
        ba->setPosition(0);
        if (!frame->header.readFrom(ba)) {
            SYLAR_LOG_INFO(g_logger) << "parse frame header failed";
            return nullptr;
        }
        if (frame->header.length > 0) {
            ba->setPosition(0);
            int ret = stream->readFixSize(ba, frame->header.length);
            if (ret <= 0) {
                SYLAR_LOG_INFO(g_logger) << "recv frame body failed, ret=" << ret << ", " << strerror(errno);
                return nullptr;
            }
            ba->setPosition(0);
        }

        switch ((FrameType)(frame->header.type)) {
            case FrameType::HEADERS: {
                frame->data = std::make_shared<HeadersFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse HeadersFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::DATA: {
                frame->data = std::make_shared<DataFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse DataFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::PRIORITY: {
                frame->data = std::make_shared<PriorityFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse PriorityFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::RST_STREAM: {
                frame->data = std::make_shared<RstFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse RstFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::SETTINGS: {
                frame->data = std::make_shared<SettingsFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse SettingsFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::PUSH_PROMISE: {
                frame->data = std::make_shared<PushPromisedFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse PushPromiseFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::PING: {
                frame->data = std::make_shared<PingFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse PingFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::GOAWAY: {
                frame->data = std::make_shared<GoAwayFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse GoAwayFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::WINDOW_UPDATE: {
                frame->data = std::make_shared<WindowUpdateFrame>();
                if (!frame->data->readFrom(ba, frame->header)) {
                    SYLAR_LOG_INFO(g_logger) << "parse WindowUpdataFrame failed";
                    return nullptr;
                }
                break;
            }
            case FrameType::CONTINUATION: {
                // TODO
                break;
            }
            default: {
                SYLAR_LOG_WARN(g_logger) << "invaild FrameType: " << (uint32_t)frame->header.type;
                return nullptr;
            }
            break;
        }
        return frame;
    } catch (std::exception& e) {
        SYLAR_LOG_ERROR(g_logger) << "FrameCodec parseFrome except: " << e.what();
    } catch (...) {
        SYLAR_LOG_ERROR(g_logger) << "FrameCodec parseFrome except";
    }
    return nullptr;
}

int32_t FrameCodec::serializeTo(Stream::ptr stream, Frame::ptr frame) {
    ByteArray::ptr ba(new ByteArray);
    frame->header.writeTo(ba);
    if (frame->data) {
        if (!frame->data->writeTo(ba, frame->header)) {
            SYLAR_LOG_ERROR(g_logger) << "FrameCodec serializeTo failed, type: " << FrameTypeToString((FrameType)frame->header.type);
            return -1;
        }
        int pos = ba->getPosition();
        ba->setPosition(0);
        frame->header.length = pos - FrameHeader::SIZE;
        frame->header.writeTo(ba);
    }
    ba->setPosition(0);
    int ret = stream->writeFixSize(ba, ba->getReadSize());
    if (ret <= 0) {
        SYLAR_LOG_ERROR(g_logger) << "FrameCodec serializeTo failed, ret: " << ret << ", " << strerror(errno);
        return -2;
    }
    return ba->getSize();
}

}
}
