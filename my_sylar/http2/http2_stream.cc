#include "http2_stream.hh"
#include "my_sylar/log.hh"

namespace sylar {
    namespace http2 {
        static const std::string CLIENT_PREFACE = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
        static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

        Http2Stream::Http2Stream(Socket::ptr sock, uint32_t init_sn) :
            AsyncSocketStream(sock, true),
            m_sn(init_sn) {
            m_codec = std::make_shared<FrameCodec>();
        }

        Http2Stream::~Http2Stream() {
            SYLAR_LOG_INFO(g_logger) << "Http2Stream::~Http2Stream " << this;
        }

        int32_t Http2Stream::sendFrame(Frame::ptr frame) {
            if (!isConnected()) {
                return -1;
            } else {
                FrameSendCtx::ptr ctx = std::make_shared<FrameSendCtx>();
                ctx->frame = frame;
                enqueue(ctx);
                return 1;
            }
        }

        AsyncSocketStream::Ctx::ptr Http2Stream::doRecv() {
            auto frame = m_codec->parseFrom(shared_from_this());
            if (!frame) {
                innerClose();
                return nullptr;
            }
            SYLAR_LOG_DEBUG(g_logger) << frame->toString();
            return nullptr;
        }

        bool Http2Stream::FrameSendCtx::doSend(AsyncSocketStream::ptr stream) {
            SYLAR_LOG_INFO(g_logger) << "do Sending...";
            return std::dynamic_pointer_cast<Http2Stream>(stream)->m_codec->serializeTo(stream, frame) > 0;
        }

        bool Http2Stream::RequestCtx::doSend(AsyncSocketStream::ptr stream) {
            SYLAR_LOG_INFO(g_logger) << "do Sending...";
            Frame::ptr headers = std::make_shared<Frame>();
            headers->header.type = (uint8_t)FrameType::HEADERS;
            headers->header.flags = (uint8_t)FrameFlagHeaders::END_HEADERS;
            if (request->getBody().empty()) {
                headers->header.flags |= (uint8_t)FrameFlagHeaders::END_STREAM;
            }
            // TODO
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
            frame->data = std::make_shared<SettingsFrame>;
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

        http::HttpResult::ptr Http2Strem::request(http::HttpRequest::ptr req, uint64_t timeout_ms) {

        }
    }
}
