#ifndef __HTTP2_STREAM_HH__
#define __HTTP2_STREAM_HH__

#include "my_sylar/stream.hh" // MUST FIRST!!!
#include "my_sylar/http2/frame.hh"
#include "my_sylar/http2/hpack.hh"
#include "my_sylar/http2/stream.hh"
#include "my_sylar/http/http.hh"
#include "my_sylar/http/http_connection.hh"

namespace sylar {
    namespace http2 {
        static const uint32_t DEFAULT_MAX_FRAME_SIZE = 16384;
        static const uint32_t MAX_MAX_FRAME_SIZE = 0XFFFFFF;
        static const uint32_t DEFAULT_HEADER_TABLE_SIZE = 4096;
        static const uint32_t DEFAULT_MAX_HEADER_LIST_SIZE = 0x400000;
        static const uint32_t DEFAULT_INITIAL_WINDOW_SIZE = 65535;
        static const uint32_t MAX_INITIAL_WINDOW_SIZE = 0XFFFFFFFF;
        static const uint32_t DEFAULT_MAX_READ_FRAME_SIZE = 1 << 20;

        struct Http2Settings {
            uint32_t header_table_size = DEFAULT_HEADER_TABLE_SIZE;
            uint32_t max_header_list_size = DEFAULT_MAX_HEADER_LIST_SIZE;
            uint32_t max_concurrent_streams = 1024;
            uint32_t max_frame_size = DEFAULT_MAX_FRAME_SIZE;
            uint32_t initial_window_size = DEFAULT_INITIAL_WINDOW_SIZE;
            bool enable_push = 0;
            std::string toString() const;
        };

        class Http2Stream : public AsyncSocketStream {
        public:
            typedef std::shared_ptr<Http2Stream> ptr;

            Http2Stream(Socket::ptr sock, bool client);
            ~Http2Stream();
            int32_t sendFrame(Frame::ptr frame);
            bool handleShakeClient();
            bool handleShakeServer();
            void handleRecvSetting(Frame::ptr frame);
            void handleSendSetting(Frame::ptr frame);

            int32_t sendGoAway(uint32_t last_stream_id, uint32_t error, const std::string& debug);
            int32_t sendSettings(const std::vector<SettingsItem>& intems);
            int32_t sendSettingsAck();
            int32_t sendRstStream(uint32_t stream_id, uint32_t error_code);
            int32_t sendPing(bool ack, uint64_t v);
            int32_t sendWindowUpdate(uint32_t stream_id, uint32_t n);

            http2::Stream::ptr newStream();
            http2::Stream::ptr newStream(uint32_t id);
            http2::Stream::ptr getStream(uint32_t id);
            void delStream(uint32_t id);

            DynamicTable& getSendTable() { return m_sendTable; }
            DynamicTable& getRecvTable() { return m_recvTable; }

            http::HttpResult::ptr request(http::HttpRequest::ptr req, uint64_t timeout_ms);

        private:
            void updateSettings(Http2Settings& sts, SettingsFrame::ptr frame);
            void handleRequest(http::HttpRequest::ptr req, http2::Stream::ptr stream);

        protected:
            struct FrameSendCtx : public Ctx {
                typedef std::shared_ptr<FrameSendCtx> ptr;
                Frame::ptr frame;
                virtual bool doSend(AsyncSocketStream::ptr stream) override;
            };
            struct RequestCtx : public Ctx {
                typedef std::shared_ptr<RequestCtx> ptr;
                http::HttpRequest::ptr request;
                http::HttpResponse::ptr response;
                std::string resultStr;
                virtual bool doSend(AsyncSocketStream::ptr stream) override;
            };
            virtual Ctx::ptr doRecv() override;
        private:
            DynamicTable m_sendTable;
            DynamicTable m_recvTable;
            FrameCodec::ptr m_codec;
            uint32_t m_sn;
            bool m_isClient;
            bool m_ssl;
            Http2Settings m_owner;
            Http2Settings m_peer;
            StreamManager m_streamMgr;
        };
    }
}

#endif

