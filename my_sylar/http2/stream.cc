#include "my_sylar/http2/stream.hh"

namespace sylar {
    namespace http2 {
        std::string Http2ErrorToString(Http2Error error) {
            return nullptr;
        }

        Stream::Stream(std::weak_ptr<Http2Stream> h2s, uint32_t id)
        : m_stream(h2s),
        m_state(State::IDLE),
        m_id(id) {
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

