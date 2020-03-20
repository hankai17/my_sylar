#ifndef __ROCK_HH__
#define __ROCK_HH__

#include <memory>
#include <functional>
#include "protocol.hh"
#include "stream.hh"

namespace sylar {
    class RockBody {
    public:
        typedef std::shared_ptr<RockBody> ptr;
        virtual ~RockBody() {}
        void setBody(const std::string& v) { m_body = v; }
        const std::string& getBody() const { return m_body; }
        virtual bool serializeToByteArray(ByteArray::ptr byte_array);
        virtual bool parseFromByteArray(ByteArray::ptr byte_array);

        template <typename T>
        std::shared_ptr<T> getAsPB() const {
            try {
                std::shared_ptr<T> data(new T);
                if (data->ParseFromString(m_body)) {
                    return data;
                }
            } catch (...) {
            }
            return nullptr;
        }

        template <typename T>
        std::shared_ptr<T> setAsPB(const T& v) const {
            try {
                return v.SerializeToString(&m_body);
            } catch (...) {
            }
            return nullptr;
        }

    protected:
        std::string         m_body;
    };

    class RockResponse;
    class RockRequest : public Request, public RockBody {
    public:
        typedef std::shared_ptr<RockRequest> ptr;
        virtual std::string toString() const override;
        virtual const std::string& getName() const override;
        virtual int32_t getType() const override;
        virtual bool serializeToByteArray(ByteArray::ptr byte_array) override;
        virtual bool parseFromByteArray(ByteArray::ptr byte_array) override;
        std::shared_ptr<RockResponse> createResponse();
    };

    class RockResponse : public Response, public RockBody {
    public:
        typedef std::shared_ptr<RockResponse> ptr;
        virtual std::string toString() const override;
        virtual const std::string& getName() const override;
        virtual int32_t getType() const override;
        virtual bool serializeToByteArray(ByteArray::ptr byte_array) override;
        virtual bool parseFromByteArray(ByteArray::ptr byte_array) override;
    };

    class RockNotify : public Notify, public RockBody {
    public:
        typedef std::shared_ptr<RockNotify> ptr;
        virtual std::string toString() const override;
        virtual const std::string& getName() const override;
        virtual int32_t getType() const override;
        virtual bool serializeToByteArray(ByteArray::ptr byte_array) override;
        virtual bool parseFromByteArray(ByteArray::ptr byte_array) override;
    };

    struct RockMsgHeader {
        RockMsgHeader();
        uint8_t     magic[2];
        uint8_t     version;
        uint8_t     flag;
        uint32_t    length;
    };

    class RockMessageDecoder : public MessageDecoder {
    public:
        typedef std::shared_ptr<RockMessageDecoder> ptr;
        virtual int32_t serializeTo(Stream::ptr stream, Message::ptr msg) override;
        virtual Message::ptr parseFrom(Stream::ptr stream) override;
    };

    struct RockResult {
        typedef std::shared_ptr<RockResult> ptr;
        RockResult(uint32_t _result, RockResponse::ptr resp)
        : result(_result),
        response(resp) {
        }
        uint32_t            result;
        RockResponse::ptr   response;
    };

    class RockStream : public sylar::AsyncSocketStream {
    public:
        typedef std::shared_ptr<RockStream> ptr;
        typedef std::function<bool(sylar::RockRequest::ptr, sylar::RockResponse::ptr,
                sylar::RockStream::ptr)> request_handler;
        typedef std::function<bool(sylar::RockNotify::ptr, sylar::RockStream::ptr)>
                notify_handler;
        RockStream(Socket::ptr sock);
        int32_t sendMessage(Message::ptr msg);
        RockResult::ptr request(RockRequest::ptr request, uint32_t timeout_ms);
        request_handler getRequestHandler() const { return m_requestHandler; }
        notify_handler getNotifyHandler() const { return m_notifyHandler; }
        void setRequestHandler(request_handler v) { m_requestHandler = v; }
        void setNotifyHandler(notify_handler v) { m_notifyHandler = v; }

    protected:
        struct RockSendCtx : public Ctx {
            typedef std::shared_ptr<RockSendCtx> ptr;
            Message::ptr        msg;
            virtual bool doSend(AsyncSocketStream::ptr stream) override;
        };

        struct RockCtx : public Ctx {
            typedef std::shared_ptr<RockCtx> ptr;
            RockRequest::ptr request;
            RockResponse::ptr response;
            virtual bool doSend(AsyncSocketStream::ptr stream) override;
        };

        virtual Ctx::ptr doRecv() override;
        void handleRequest(RockRequest::ptr req);
        void handleNotify(RockNotify::ptr noti);
    private:
        request_handler             m_requestHandler;
        notify_handler              m_notifyHandler;
        RockMessageDecoder::ptr     m_decoder;

    };

    class RockSession : public RockStream {
    public:
        typedef std::shared_ptr<RockSession> ptr;
        RockSession(Socket::ptr sock);
    };

    class RockConnection : public RockStream {
    public:
        typedef std::shared_ptr<RockConnection> ptr;
        RockConnection();
        bool connect(sylar::Address::ptr addr);
    };
}

#endif