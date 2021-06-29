#include "my_sylar/http2/http2_stream.hh"
#include "my_sylar/log.hh"
#include "my_sylar/util.hh"
#include "my_sylar/scheduler.hh"
#include "my_sylar/iomanager.hh"

namespace sylar {
    namespace http2 {
        static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
        static const std::vector<std::string> s_http2error_strings = {
            "OK",
            "PROTOCOL_ERROR",
            "INTERNAL_ERROR",
            "FLOW_CONTROL_ERROR",
            "SETTINGS_TIMEOUT_ERROR",
            "STREAM_CLOSED_ERROR",
            "FRAME_SIZE_ERROR",
            "REFUSED_FRAME_ERROR",
            "CANCEL_ERROR",
            "COMPRESSION_ERROR",
            "CONNECT_ERROR",
            "ENHANCE_YOUR_CLAM_ERROR",
            "inadequate_SECURITY_ERROR",
            "HTTP11_REQUIRE_ERROR"
        };

        static const std::string CLIENT_PREFACE = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
        std::string Http2Settings::toString() const {
            std::stringstream ss;
            ss << "[Http2Settings header_table_size: " << header_table_size
            << " max_header_list_size: " << max_header_list_size
            << " max_concurrent_streams: " << max_concurrent_streams
            << " max_frame_size: " << max_frame_size
            << " initial_window_size: " << initial_window_size
            << " enable_push: " << enable_push << "]";
            return ss.str();
        }

        Http2Stream::Http2Stream(Socket::ptr sock, bool client) :
            AsyncSocketStream(sock, true),
            m_sn(client ? -1 : 0),
            m_isClient(client),
            m_ssl(false) {
            m_codec = std::make_shared<FrameCodec>();
        }

        Http2Stream::~Http2Stream() {
            SYLAR_LOG_INFO(g_logger) << "Http2Stream::~Http2Stream " << this;
        }

        void Http2Stream::updateSettings(Http2Settings& sts, SettingsFrame::ptr frame) {
            DynamicTable& table = &sts == &m_owner ? m_sendTable : m_recvTable;
            for (const auto& i : frame->items) {
                switch ((SettingsFrame::Settings)i.identifier) {
                    case SettingsFrame::Settings::HEADER_TABLE_SIZE : {
                        sts.header_table_size = i.value;
                        table.setMaxDataSize(sts.header_table_size);
                        break;
                    }
                    case SettingsFrame::Settings::ENABLE_PUSH : {
                        if (i.value != 0 && i.value != 1) {
                            SYLAR_LOG_ERROR(g_logger) << "invaild enable_push: " << i.value;
                            sendGoAway(m_sn, (uint32_t)Http2Error::PROTOCOL_ERROR, "");
                        }
                        sts.enable_push = i.value;
                        break;
                    }
                    case SettingsFrame::Settings::MAX_CONCURRENT_STREAMS : {
                        sts.max_concurrent_streams = i.value;
                        break;
                    }
                    case SettingsFrame::Settings::INITIAL_WINDOW_SIZE : {
                        sts.initial_window_size = i.value;
                        break;
                    }
                    case SettingsFrame::Settings::MAX_FRAME_SIZE : {
                        sts.max_frame_size = i.value;
                        if (sts.max_frame_size < DEFAULT_MAX_FRAME_SIZE ||
                            sts.max_frame_size > MAX_MAX_FRAME_SIZE) {
                            SYLAR_LOG_ERROR(g_logger) << "invaild max_frame_size: " << i.value;
                            sendGoAway(m_sn, (uint32_t)Http2Error::PROTOCOL_ERROR, "");
                        }
                        break;
                    }
                    case SettingsFrame::Settings::MAX_HEADER_LIST_SIZE : {
                        sts.max_header_list_size = i.value;
                        break;
                    }
                    default: {
                        break;
                    }
                }
            }
        }

        void Http2Stream::handleRequest(http::HttpRequest::ptr req, http2::Stream::ptr stream) {
            http::HttpResponse::ptr rsp = std::make_shared<http::HttpResponse>(req->getVersion(), false);
            //SYLAR_LOG_DEBUG(g_logger) << *req;
            //rsp->setHeader("server", "my_sylar");
            //m_server
            // TODO

        }

        void Http2Stream::handleRecvSetting(Frame::ptr frame) {
            auto s = std::dynamic_pointer_cast<SettingsFrame>(frame->data);
            SYLAR_LOG_DEBUG(g_logger) << "handleRecvSetting " << s->toString();
            updateSettings(m_owner, s);
        }

        void Http2Stream::handleSendSetting(Frame::ptr frame) {

        }

        int32_t Http2Stream::sendGoAway(uint32_t last_stream_id, uint32_t error, const std::string& debug) {
            Frame::ptr frame = std::make_shared<Frame>();
            frame->header.type = (uint8_t)FrameType::GOAWAY;
            GoAwayFrame::ptr data = std::make_shared<GoAwayFrame>();
            frame->data = data;
            data->last_stream_id = last_stream_id;
            data->error_code = error;
            data->data = debug;
            return sendFrame(frame);
        }

        int32_t Http2Stream::sendSettings(const std::vector<SettingsItem>& items) {
            Frame::ptr frame = std::make_shared<Frame>();
            frame->header.type = (uint8_t)FrameType::SETTINGS;
            SettingsFrame::ptr data = std::make_shared<SettingsFrame>();
            frame->data = data;
            data->items = items;
            return sendFrame(frame);
        }

        int32_t Http2Stream::sendSettingsAck() {
            Frame::ptr frame = std::make_shared<Frame>();
            frame->header.type = (uint8_t)FrameType::SETTINGS;
            frame->header.flags = (uint8_t)FrameFlagSettings::ACK;
            return sendFrame(frame);
        }

        int32_t Http2Stream::sendRstStream(uint32_t stream_id, uint32_t error_code) {
            return 0;
        }

        int32_t Http2Stream::sendPing(bool ack, uint64_t v) {
            Frame::ptr frame = std::make_shared<Frame>();
            frame->header.type = (uint8_t)FrameType::PING;
            if (ack) {
                frame->header.flags = (uint8_t)FrameFlagPing::ACK;
            }
            PingFrame::ptr data = std::make_shared<PingFrame>();
            frame->data = data;
            data->uint64 = v;
            return sendFrame(frame);
        }

        int32_t Http2Stream::sendWindowUpdate(uint32_t stream_id, uint32_t n) {
            Frame::ptr frame = std::make_shared<Frame>();
            frame->header.type = (uint8_t)FrameType::WINDOW_UPDATE;
            frame->header.identifier = stream_id;
            WindowUpdateFrame::ptr data = std::make_shared<WindowUpdateFrame>();
            frame->data = data;
            data->increment = n;
            return sendFrame(frame);
        }

        int32_t Http2Stream::sendFrame(Frame::ptr frame) {
            if (isConnected()) {
                FrameSendCtx::ptr ctx = std::make_shared<FrameSendCtx>();
                ctx->frame = frame;
                enqueue(ctx);
                return 1;
            } else {
                return -1;
            }
        }

        AsyncSocketStream::Ctx::ptr Http2Stream::doRecv() {
            auto frame = m_codec->parseFrom(shared_from_this());
            if (!frame) {
                innerClose();
                return nullptr;
            }
            SYLAR_LOG_DEBUG(g_logger) << frame->toString();
            if (frame->header.identifier) {
                auto stream = getStream(frame->header.identifier);
                if (!stream) {
                    if (m_isClient) {
                        SYLAR_LOG_ERROR(g_logger) << "doRecv streamId: " << frame->header.identifier
                                                  << " not exist, " << frame->toString();
                        return nullptr;
                    } else {
                        stream = newStream(frame->header.identifier);
                        if (!stream) {
                            sendGoAway(m_sn, (uint32_t) Http2Error::PROTOCOL_ERROR, "");
                            return nullptr;
                        }
                    }
                }
                stream->handleFrame(frame, m_isClient);
                if (stream->getStat() == http2::Stream::State::CLOSED) {
                    if (m_isClient) {
                        delStream(stream->getId());
                        RequestCtx::ptr ctx = getAndDelCtx<RequestCtx>(stream->getId());
                        if (!ctx) {
                            SYLAR_LOG_WARN(g_logger) << "Http2Stream request timeout response";
                            return nullptr;
                        }
                        ctx->response = stream->getResponse();
                        return ctx;
                    } else {
                        auto req = stream->getRequest();
                        if (!req) {
                            SYLAR_LOG_DEBUG(g_logger) << "Http2Stream recv http faild, errno: " << errno
                                                      << " strerror: " << strerror(errno);
                            sendGoAway(m_sn, (uint32_t) Http2Error::PROTOCOL_ERROR, "");
                            delStream(stream->getId());
                            return nullptr;
                        }
                        m_iomanager->schedule(std::bind(&Http2Stream::handleRequest, this, req, stream));
                    }
                }
            } else {
                if (frame->header.type == (uint8_t)FrameType::SETTINGS) {
                    if (!(frame->header.flags & (uint8_t)FrameFlagSettings::ACK)) {
                        handleRecvSetting(frame);
                        sendSettingsAck();
                    }
                } else if (frame->header.type == (uint8_t)FrameType::PING) {
                    if (!(frame->header.flags & (uint8_t)FrameFlagPing::ACK)) {
                        auto data = std::dynamic_pointer_cast<PingFrame>(frame->data);
                        sendPing(true, data->uint64);
                    }
                }
            }
            return nullptr;
        }

        bool Http2Stream::FrameSendCtx::doSend(AsyncSocketStream::ptr stream) {
            SYLAR_LOG_INFO(g_logger) << "do Sending not-http2-protocol-data...";
            return std::dynamic_pointer_cast<Http2Stream>(stream)->m_codec->serializeTo(stream, frame) > 0;
        }

        bool Http2Stream::RequestCtx::doSend(AsyncSocketStream::ptr stream) {
            auto h2stream = std::dynamic_pointer_cast<Http2Stream>(stream);

            Frame::ptr headers = std::make_shared<Frame>();
            // init head
            headers->header.type = (uint8_t)FrameType::HEADERS;
            headers->header.flags = (uint8_t)FrameFlagHeaders::END_HEADERS;
            if (request->getBody().empty()) {
                headers->header.flags |= (uint8_t)FrameFlagHeaders::END_STREAM;
            }
            headers->header.identifier = sn;

            // init data
            HeadersFrame::ptr data = std::make_shared<HeadersFrame>();
            HPack hp(h2stream->m_sendTable);
            std::vector<std::pair<std::string, std::string>> hs;
            //hs.push_back(std::make_pair(":stream_id", std::to_string(sn)));
            auto m = request->getHeaders();
            for (const auto& i : m) {
                /*
                HeaderField hf;
                hf.type = IndexType::NERVER_INDEXED_NEW_NAME;
                hf.name = i.first;
                hf.value = i.second;
                data->fields.push_back(hf);
                */
                hs.push_back(std::make_pair((i.first), i.second));
            }
            hp.pack(hs, data->data);
            headers->data = data;

            bool ok = std::dynamic_pointer_cast<Http2Stream>(stream)->m_codec->serializeTo(stream, headers) > 0;
            if (!ok) {
                SYLAR_LOG_INFO(g_logger) << "doSend send headerFrame failed";
                return ok;
            }
            if (!request->getBody().empty()) {
                Frame::ptr body = std::make_shared<Frame>();
                body->header.type = (uint8_t)FrameType::DATA;
                body->header.flags = (uint8_t)FrameFlagData::END_STREAM;
                body->header.identifier = sn;
                auto data = std::make_shared<DataFrame>();
                data->data = request->getBody();
                body->data = data;
                ok = std::dynamic_pointer_cast<Http2Stream>(stream)->m_codec->serializeTo(stream, body) > 0;
            }
            return ok;
        }

        bool Http2Stream::handleShakeClient() {
            if (!isConnected()) {
                return false;
            }
            int ret = writeFixSize(CLIENT_PREFACE.c_str(), CLIENT_PREFACE.size());
            if (ret <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "handleShakeClient CLIENT_PREFACE failed, ret = " << ret
                    << ", errno: " << errno << ", strerror: " << strerror(errno);
                return false;
            }
            Frame::ptr frame = std::make_shared<Frame>();
            frame->header.type = (uint8_t)FrameType::SETTINGS;
            auto data = std::make_shared<SettingsFrame>();

            SettingsItem item;
            item.identifier = 0x3;
            item.value = 1024;
            data->items.push_back(item);

            item.identifier = 0x5;
            item.value = 16384;
            data->items.push_back(item);

            frame->data = data;

            ret = m_codec->serializeTo(shared_from_this(), frame);
            if (ret <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "handleShakeClient Settings failed, ret = " << ret
                                          << ", errno: " << errno << ", strerror: " << strerror(errno);
                return false;
            }
            return true;
        }

        bool Http2Stream::handleShakeServer() {
            ByteArray::ptr ba = std::make_shared<ByteArray>();
            int ret = readFixSize(ba, CLIENT_PREFACE.size());
            if (ret <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "handleShakeServer recv CLIENT_PREFACE failed, ret = " << ret
                                          << ", errno: " << errno << ", strerror: " << strerror(errno);
                return false;
            }
            ba->setPosition(0);
            if (ba->toString() != CLIENT_PREFACE) {
                SYLAR_LOG_ERROR(g_logger) << "handleShakeServer recv CLIENT_PREFACE failed, ret = " << ret
                                          << ", errno: " << errno << ", strerror: " << strerror(errno);
                return false;
            }
            auto frame = m_codec->parseFrom(shared_from_this());
            if (!frame) {
                SYLAR_LOG_ERROR(g_logger) << "handleShakeServer codec parsed failed "
                                          << ", errno: " << errno << ", strerror: " << strerror(errno);
                return false;
            }
            if (frame->header.type != (uint8_t)FrameType::SETTINGS) {
                SYLAR_LOG_ERROR(g_logger) << "handleShakeServer codec header type not settings "
                                          << ", errno: " << errno << ", strerror: " << strerror(errno);
                return false;
            }
            return true;
        }

        http2::Stream::ptr Http2Stream::newStream() {
            http2::Stream::ptr stream = std::make_shared<http2::Stream>(
                    std::dynamic_pointer_cast<Http2Stream>(shared_from_this()),
                    sylar::Atomic::addFetch(m_sn, 2)
                    );
            m_streamMgr.add(stream);
            return stream;
        }

        http2::Stream::ptr Http2Stream::newStream(uint32_t id) {
            if (id <= m_sn) {
                return nullptr;
            }
            m_sn = id;
            http2::Stream::ptr stream = std::make_shared<http2::Stream>(
                    std::dynamic_pointer_cast<Http2Stream>(shared_from_this()), id);
            m_streamMgr.add(stream);
            return stream;
        }

        http2::Stream::ptr Http2Stream::getStream(uint32_t id) {
            return m_streamMgr.get(id);
        }

        void Http2Stream::delStream(uint32_t id) {
            m_streamMgr.del(id);
        }

        http::HttpResult::ptr Http2Stream::request(http::HttpRequest::ptr req, uint64_t timeout_ms) {
            if (isConnected()) {
                RequestCtx::ptr ctx = std::make_shared<RequestCtx>();
                auto stream = newStream();
                ctx->request = req;
                ctx->sn = stream->getId();
                ctx->timeout = timeout_ms;
                ctx->scheduler = sylar::Scheduler::GetThis();
                ctx->fiber = sylar::Fiber::GetThis();
                addCtx(ctx);
                ctx->timer = sylar::IOManager::GetThis()->addTimer(timeout_ms,
                        std::bind(&Http2Stream::onTimeOut, shared_from_this(), ctx));
                enqueue(ctx);
                sylar::Fiber::YeildToHold();
                auto ret = std::make_shared<http::HttpResult>(ctx->result, ctx->response, ctx->resultStr);
                if (ret->result == 0 && !ctx->response) {
                    ret->result = -401;
                    ret->error = "rst_stream";
                }
                return ret;
            } else {
                return std::make_shared<http::HttpResult>(AsyncSocketStream::NOT_CONNECT, nullptr, "not connect getRemoteAddr");
            }
        }
    }
}
