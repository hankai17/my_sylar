#ifndef __H2_STREAM_HH__
#define __H2_STREAM_HH__

#include <unordered_map>
#include "my_sylar/thread.hh"

namespace sylar {
    namespace http2 {
        class Http2Stream;
        /*
                                +--------+
                        send PP |        | recv PP
                       ,--------|  idle  |--------.
                      /         |        |         \
                     v          +--------+          v
              +----------+          |           +----------+
              |          |          | send H /  |          |
       ,------| reserved |          | recv H    | reserved |------.
       |      | (local)  |          |           | (remote) |      |
       |      +----------+          v           +----------+      |
       |          |             +--------+             |          |
       |          |     recv ES |        | send ES     |          |
       |   send H |     ,-------|  open  |-------.     | recv H   |
       |          |    /        |        |        \    |          |
       |          v   v         +--------+         v   v          |
       |      +----------+          |           +----------+      |
       |      |   half   |          |           |   half   |      |
       |      |  closed  |          | send R /  |  closed  |      |
       |      | (remote) |          | recv R    | (local)  |      |
       |      +----------+          |           +----------+      |
       |           |                |                 |           |
       |           | send ES /      |       recv ES / |           |
       |           | send R /       v        send R / |           |
       |           | recv R     +--------+   recv R   |           |
       | send R /  `----------->|        |<-----------'  send R / |
       | recv R                 | closed |               recv R   |
       `----------------------->|        |<----------------------'
                                +--------+

          send:   endpoint sends this frame
          recv:   endpoint receives this frame

          H:  HEADERS frame (with implied CONTINUATIONs)
          PP: PUSH_PROMISE frame (with implied CONTINUATIONs)
          ES: END_STREAM flag
          R:  RST_STREAM frame
*/
        enum class Http2Error {
            OK                      = 0x0,
            PROTOCOL_ERROR          = 0x1,
            INTERNAL_ERROR          = 0x2,
            FLOW_CONTROL_ERROR      = 0x3,
            SETTINGS_TIMEOUT_ERROR  = 0x4,
            STREAM_CLOSED_ERROR     = 0x5,
            FRAME_SIZE_ERROR        = 0x6,
            REFUSED_STREAM_ERROR    = 0x7,
            CANCEL_ERROR            = 0x8,
            COMPRESSION_ERROR       = 0x9,
            CONNECT_ERROR           = 0xa,
            ENHANCE_YOUR_CLAM_ERROR = 0xb,
            inadequate_SECURITY_ERROR = 0xc,
            HTTP11_REQUEIRED_ERROR  = 0xd,
        };
        std::string Http2ErrorToString(Http2Error error);

        class Stream {
        public:
            typedef std::shared_ptr<Stream> ptr;
            enum class State {
                IDLE                = 0x0,
                OPEN                = 0x1,
                CLOSED              = 0x2,
                RESERVED_LOCAL      = 0x3,
                RESERVED_REMOTE     = 0x4,
                HALF_CLOSED_LOCAL   = 0x5,
                HALF_CLOSED_REMOTE  = 0x6
            };
            Stream(std::weak_ptr<Http2Stream> h2s, uint32_t id);
            uint32_t getId() const { return m_id; }
            //static std::string StateToString(State state);
            //int32_t handleFrame(Frame::ptr frame, bool is_client);

        private:
            std::weak_ptr<Http2Stream>    m_stream; // why use weak ?
            State   m_state;
            uint32_t m_id;
        };

        class StreamManager {
        public:
            typedef std::shared_ptr<StreamManager> ptr;
            typedef sylar::RWMutex RWMutexType;

            Stream::ptr get(uint32_t id);
            void add(Stream::ptr stream);
            void del(uint32_t id);

        private:
            RWMutexType m_mutex;
            std::unordered_map<uint32_t, Stream::ptr> m_streams;
        };
    }
}

#endif

