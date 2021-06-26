#ifndef __H2_STREAM_HH__
#define __H2_STREAM_HH__

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
            Stream(std::shared_ptr<Http2Stream> h2s);

        private:
            std::shared_ptr<Http2Stream>    m_stream;
            State   m_state;
            uint32_t m_id;
        };
    }
}

#endif

