#ifndef __STREAM_HH__
#define __STREAM_HH__

#include <memory>
#include "bytearray.hh"
#include "socket.hh"

namespace sylar {
    class Stream {
    public:
        typedef std::shared_ptr<Stream> ptr;
        virtual ~Stream() {}; // Can not virutal ~Stream();

        virtual int read(void* buffer, size_t length) = 0;
        virtual int read(ByteArray::ptr ba, size_t length) = 0;
        virtual int readFixSize(void* buffer, size_t length);
        virtual int readFixSize(ByteArray::ptr ba, size_t length);
        virtual int write(const char* buffer, size_t length) = 0;
        virtual int write(ByteArray::ptr ba, size_t length) = 0;
        virtual int writeFixSize(const char* buffer, size_t length);
        virtual int writeFixSize(ByteArray::ptr ba, size_t length);
        virtual void close() = 0;
    };

    class SocketStream : public Stream {
    public:
        typedef std::shared_ptr<SocketStream> ptr;
        SocketStream(Socket::ptr sock, bool owner = true);
        ~SocketStream();

        virtual int read(void* buffer, size_t length) override;
        virtual int read(ByteArray::ptr ba, size_t length) override;
        virtual int write(const char* buffer, size_t length) override;
        virtual int write(ByteArray::ptr ba, size_t length) override;
        virtual void close() override;

        Socket::ptr getSocket() const { return m_socket; }
        bool isConnected() const;

    private:
        Socket::ptr         m_socket;
        bool                m_owner;
    };

}

#endif
