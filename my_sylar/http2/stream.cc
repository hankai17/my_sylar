#include "my_sylar/log.hh"
#include "my_sylar/util.hh"
#include "my_sylar/http2/stream.hh"
#include "my_sylar/http2/http2_stream.hh"

namespace sylar {
    namespace http2 {
        static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

        std::string Http2ErrorToString(Http2Error error) {
            return nullptr;
        }

        Stream::Stream(std::weak_ptr<Http2Stream> h2s, uint32_t id)
        : m_stream(h2s),
        m_state(State::IDLE),
        m_id(id) {
        }

        int32_t Stream::handleHeadersFrame(Frame::ptr frame, bool is_client) {
            auto headers = std::dynamic_pointer_cast<HeadersFrame>(frame->data);
            if (!headers) {
                SYLAR_LOG_ERROR(g_logger) << "stream_id: " << m_id
                << " handleHeadersFrame data not HeaderFrame " << frame->toString();
                return -1;
            }
            auto stream = getStream();
            if (!stream) {
                SYLAR_LOG_ERROR(g_logger) << "stream_id: " << m_id
                << " handleHeaderFrame stream is closed " << frame->toString();
                return -2;
            }
            if (!m_recvHPack) {
                m_recvHPack = std::make_shared<HPack>(stream->getRecvTable());
            }
            return m_recvHPack->parse(headers->data);
        }

        int32_t Stream::handleDataFrame(Frame::ptr frame, bool is_client) {
            auto data = std::dynamic_pointer_cast<DataFrame>(frame->data);
            if (!data) {
                SYLAR_LOG_ERROR(g_logger) << "stream_id: " << m_id
                                          << " handleDataFrame data not DataFrame " << frame->toString();
                return -1;
            }
            auto stream = getStream();
            if (!stream) {
                SYLAR_LOG_ERROR(g_logger) << "stream_id: " << m_id
                                          << " handleDataFrame stream is closed " << frame->toString();
                return -2;
            }
            if (is_client) {
                m_response = std::make_shared<http::HttpResponse>(0x20);
                m_response->setBody(data->data);
            } else {
                m_request = std::make_shared<http::HttpRequest>(0x20);
                m_request->setBody(data->data);
            }
            return 0;
        }

        int32_t Stream::handleRstStreamFrame(Frame::ptr frame, bool is_client) {
            m_state = State::CLOSED;
            return 0;
        }

        int32_t Stream::handleFrame(Frame::ptr frame, bool is_client) {
            int ret = 0;
            if (frame->header.type == (uint8_t)FrameType::HEADERS) {
                ret = handleHeadersFrame(frame, is_client);
            } else if (frame->header.type == (uint8_t)FrameType::DATA) {
                ret = handleDataFrame(frame, is_client);
            } else if (frame->header.type == (uint8_t)FrameType::RST_STREAM) {
                ret = handleRstStreamFrame(frame, is_client);
            }
            if (frame->header.flags & (uint8_t)FrameFlagHeaders::END_STREAM) {
                m_state = State::CLOSED;
                if (is_client) {
                    if (!m_response) {
                        m_response = std::make_shared<http::HttpResponse>(0x20);
                    }
                    if (m_recvHPack) {
                        auto& m = m_recvHPack->getHeaders();
                        for (const auto& i : m) {
                            m_response->setHeader(i.name, i.value);
                        }
                    }
                    m_response->setStatus((http::HttpStatus)sylar::TypeUtil::Atoi(m_response->getHeader(":status")));
                } else {
                    if (!m_request) {
                        m_request = std::make_shared<http::HttpRequest>(0x20);
                    }
                    if (m_recvHPack) {
                        auto& m = m_recvHPack->getHeaders();
                        for (const auto& i : m) {
                            m_request->setHeader(i.name, i.value);
                        }
                    }
                    m_request->setMethod(http::CharsToHttpMethod(m_request->getHeader(":method").c_str()));
                    if (m_request->hasHeader(":path", nullptr)) {
                        m_request->setPath(m_request->getHeader(":path"));
                        SYLAR_LOG_INFO(g_logger) << m_request->getPath() << "-" << m_request->getQuery();
                    }
                    SYLAR_LOG_DEBUG(g_logger) << "id: " << m_id << " is_client: " << is_client <<
                    " req: " << m_request << " resp: " << m_response;
                }
            }
            return ret;
        }

        int32_t Stream::sendFrame(Frame::ptr frame) {
            return 0;
        }

        int32_t Stream::sendResponse(http::HttpResponse::ptr rsp) {
            auto stream = getStream(); if (!stream) {
                SYLAR_LOG_ERROR(g_logger) << "stream_id: " << m_id
                << " sendResponse stream is closed";
                return -1;
            }
            rsp->setHeader(":status", std::to_string(uint32_t(rsp->getStatus())));
            Frame::ptr headers = std::make_shared<Frame>();
            headers->header.type = (uint8_t)FrameType::HEADERS;
            headers->header.flags = (uint8_t)FrameFlagHeaders::END_HEADERS;
            if (rsp->getBody().empty()) {
                headers->header.flags |= (uint8_t)FrameFlagHeaders::END_STREAM;
            }
            headers->header.identifier = m_id;
            HeadersFrame::ptr data;
            auto m = rsp->getHeaders();
            data = std::make_shared<HeadersFrame>();

            HPack hp(stream->getSendTable());
            std::vector<std::pair<std::string, std::string>> hs;
            for (const auto& i : m) {
                hs.push_back(std::make_pair(i.first, i.second));
            }
            hp.pack(hs, data->data);

            headers->data = data;
            int ok = stream->sendFrame(headers);
            if (ok < 0) {
                SYLAR_LOG_ERROR(g_logger) << "stream_id: " << m_id
                << " saendResponse send Headers faild";
                return ok;
            }
            if (!rsp->getBody().empty()) {
                Frame::ptr body = std::make_shared<Frame>();
                body->header.type = (uint8_t)FrameType::DATA;
                body->header.flags = (uint8_t)FrameFlagData::END_STREAM;
                body->header.identifier = m_id;
                auto data = std::shared_ptr<DataFrame>();
                data->data = rsp->getBody();
                body->data = data;
                ok = stream->sendFrame(body);
            }
            return ok;
        }

        Stream::ptr StreamManager::get(uint32_t id) {
            RWMutexType::ReadLock lock(m_mutex);
            auto it = m_streams.find(id);
            return it == m_streams.end() ? nullptr : it->second;
        }

        void StreamManager::add(Stream::ptr stream) {
            RWMutexType::WriteLock lock(m_mutex);
            m_streams[stream->getId()] = stream;
        }

        void StreamManager::del(uint32_t id) {
            RWMutexType::WriteLock lock(m_mutex);
            m_streams.erase(id);
        }
    }
}

