#ifndef __PROTOCOL_HH__
#define __PROTOCOL_HH__

#include <memory>
#include "bytearray.hh"
#include "stream.hh"
#include <string>
#include <google/protobuf/message.h>

namespace sylar {
    class Message {
    public:
        typedef std::shared_ptr<Message> ptr;
        enum MessageType {
            REQUEST = 1,
            RESPONSE = 2,
            NOTIFY = 3,
        };
        virtual ~Message() {}
        virtual ByteArray::ptr toByteArray();
        virtual bool serializeToByteArray(ByteArray::ptr byte_array) = 0;
        virtual bool parseFromByteArray(ByteArray::ptr byte_array) = 0;
        virtual std::string toString() const = 0;
        virtual const std::string& getName() const = 0;
        virtual int32_t getType() const = 0;
    };

    class MessageDecoder {
    public:
        typedef std::shared_ptr<MessageDecoder> ptr;
        virtual ~MessageDecoder() {}
        virtual int32_t serializeTo(Stream::ptr stream, Message::ptr msg) = 0;
        virtual Message::ptr parseFrom(Stream::ptr stream) = 0;
    };

    class Request : public Message { // It is a abstract class Can not new it!
    public:
        typedef std::shared_ptr<Request> ptr;
        Request();
        uint32_t getSn() const { return m_sn; }
        uint32_t getCmd() const { return m_cmd; }
        void setSn(uint32_t v) { m_sn = v; }
        void setCmd(uint32_t v) { m_cmd = v; }
        virtual bool serializeToByteArray(ByteArray::ptr byte_array) override; // Must do it in .cc
        virtual bool parseFromByteArray(ByteArray::ptr byte_array) override;

    protected:
        uint32_t        m_sn;
        uint32_t        m_cmd;
    };

    class Response : public Message {
    public:
        typedef std::shared_ptr<Response> ptr;
        Response();
        uint32_t getSn() const { return m_sn; }
        uint32_t getCmd() const { return m_cmd; }
        uint32_t getResult() const { return m_result; }
        const std::string& getResultStr() const { return m_resultStr; }
        void setSn(uint32_t v) { m_sn = v; }
        void setCmd(uint32_t v) { m_cmd = v; }
        void setResult(uint32_t v) { m_result = v; }
        void setResultStr(const std::string& v) { m_resultStr = v; }
        virtual bool serializeToByteArray(ByteArray::ptr byte_array) override;
        virtual bool parseFromByteArray(ByteArray::ptr byte_array) override;

    protected:
        uint32_t        m_sn;
        uint32_t        m_cmd;
        uint32_t        m_result;
        std::string     m_resultStr;
    };

    class Notify : public Message {
    public:
        typedef std::shared_ptr<Notify> ptr;
        Notify();
        uint32_t getNotify() const { return m_notify; }
        void setNotify(uint32_t v) { m_notify = v; }
        virtual bool serializeToByteArray(ByteArray::ptr byte_array);
        virtual bool parseFromByteArray(ByteArray::ptr byte_array);

    protected:
        uint32_t        m_notify;
    };

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

    class RockRequest : public Request, public RockBody {
    public:
        typedef std::shared_ptr<RockRequest> ptr;
        virtual std::string toString() const override;
        virtual const std::string& getName() const override;
        virtual int32_t getType() const override;
        virtual bool serializeToByteArray(ByteArray::ptr byte_array) override;
        virtual bool parseFromByteArray(ByteArray::ptr byte_array) override;
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

}

#endif