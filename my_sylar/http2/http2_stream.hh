#ifndef __HTTP2_STREAM_HH__
#define __HTTP2_STREAM_HH__

#include "my_sylar/stream.hh" // MUST FIRST!!!
#include "frame.hh"
#include "hpack.hh"
#include "stream.hh"
#include "my_sylar/http/http.hh"
#include "my_sylar/http/http_connection.hh"

namespace sylar {
    namespace http2 {
        class Http2Stream : public AsyncSocketStream {
        public:
            typedef std::shared_ptr<Http2Stream> ptr;
            Http2Stream(Socket::ptr sock, uint32_t init_sn);
            ~Http2Stream();
            int32_t sendFrame(Frame::ptr frame);
            bool handleShakeClient();
            bool handleShakeServer();
            http::HttpResult::ptr request(http::HttpRequest::ptr req, uint64_t timeout_ms);

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
        };

    }
}

#endif

