#ifndef __WS_CONNECTION_HH__
#define __WS_CONNECTION_HH__

#include "socket.hh"
#include "http/http.hh"
#include "http/http_connection.hh"
#include "uri.hh"
#include <memory>
#include <map>
#include <string>

namespace sylar {
    namespace http {
#pragma pack(1)
        struct WSFrameHead {
            enum OPCODE {
                CONTINUE    = 0,
                TEXT_FRAME  = 1,
                BIN_FRAME   = 2,
                CLOSE_FRAME = 8,
                PING_FRAME  = 0x9,
                PONG_FRAME  = 0xA
            };
            uint32_t opcode: 4;
            bool rsv3: 1;
            bool rsv2: 1;
            bool rsv1: 1;
            bool fin: 1;
            uint32_t payload: 7;
            bool mask: 1;
            std::string toString() const;
        };
#pragma pack()

        class WSFrameMessage {
        public:
            typedef std::shared_ptr<WSFrameMessage> ptr;
            WSFrameMessage(int opcode = 0, const std::string& data = "");

            int getOpcode() const { return m_opcode; }
            void setOpcode(int value) { m_opcode = value; }
            std::string& getData() { return m_data; }
            void setData(const std::string& data) { m_data = data; }

        private:
            int             m_opcode;
            std::string     m_data;
        };

        class WSConnection : public HttpConnection {
        public:
            typedef std::shared_ptr<WSConnection> ptr;
            WSConnection(sylar::Socket::ptr sock, bool owner = true);
            static std::pair<HttpResult::ptr, WSConnection::ptr> Create(const std::string& url,
                    uint64_t timeout_ms,
                    const std::map<std::string, std::string>& headers = {});
            static std::pair<HttpResult::ptr, WSConnection::ptr> Create(Uri::ptr uri,
                   uint64_t timeout_ms,
                   const std::map<std::string, std::string>& headers = {});
            WSFrameMessage::ptr recvMessage();
            int32_t sendMessage(WSFrameMessage::ptr msg, bool fin = true);
            int32_t sendMessage(const std::string& msg, int32_t opcode = WSFrameHead::TEXT_FRAME, bool fin = true);
            int32_t ping();
            int32_t pong();

        private:
        };

        WSFrameMessage::ptr WSRecvMessage(Stream* stream, bool client);
        int32_t WSSendMessage(Stream* stream, WSFrameMessage::ptr msg, bool client, bool fin);
        int32_t WSPing(Stream* stream);
        int32_t WSPong(Stream* stream);
    }
}


#endif
