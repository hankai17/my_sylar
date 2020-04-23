#ifndef __BUFFER_HH__
#define __BUFFER_HH__

#include <memory>
#include <vector>

/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size

namespace sylar {
    class Buffer {
    public:
        typedef std::shared_ptr<Buffer> ptr;
        static const size_t kCheapPrepend = 8;
        static const size_t kInitialSize = 1024;

        Buffer(size_t initialSize = kInitialSize);
        void swap(Buffer& buffer);

        size_t prependableBytes() const { return m_readIndex - 0; }
        size_t readableBytes() const { return m_writeIndex - m_readIndex; }
        size_t writableBytes() const { return m_buffer.size() - m_writeIndex; }

        // 接收数据
        void hasWritten(size_t length);
        void ensuerWritableBytes(size_t length);
        void append(const char* data, size_t length); // 0
        void append(const void* data, size_t length) { append(static_cast<const char*>(data), length); }
        //void append(const StringPiece& str) { append(str.data(), str.size()); }
        void appendInt64(int64_t x);
        void appendInt32(int32_t x);
        void appendInt16(int16_t x);
        void appendInt8(int8_t x);

        // 消费置位
        void retrieveAll() { m_readIndex = kCheapPrepend; m_writeIndex = kCheapPrepend; }
        void retrieve(size_t length);
        void retrieveUntil(const char* end);
        void retrieveInt64() { retrieve(sizeof(int64_t)); }
        void retrieveInt32() { retrieve(sizeof(int32_t)); }
        void retrieveInt16() { retrieve(sizeof(int16_t)); }
        void retrieveInt8() { retrieve(sizeof(int8_t)); }
        std::string retrieveAsString(size_t length);
        std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }

        // 消费  // 消费上层自己写 此类不提供 // 1
        int64_t readInt64();
        int32_t readInt32();
        int16_t readInt16();
        int8_t readInt8();

        // 往对头加元素 危
        void prepend(const void* data, size_t length);
        void prependInt64(int64_t x);
        void prependInt32(int32_t x);
        void prependInt16(int16_t x);
        void prependInt8(int8_t x);

        void shrink(size_t reserve);
        size_t internalCap() const { return m_buffer.capacity(); }
        ssize_t readFd(int fd, int* savedErrno);
        ssize_t orireadFd(int fd, int* savedErrno);
        ssize_t orireadFd(int fd, size_t length, int* savedErrno);
        ssize_t writeFd(int fd, size_t length, int* savedErrno);

        //char* beginWrite() const { return begin() + m_writeIndex; } // WHY not const ?
        char* beginWrite() { return begin() + m_writeIndex; }
        char* peek() { return begin() + m_readIndex; }

        //const char* findCRLF() const {}
        //const char* findCRLF(const char* start) const {}
        //const char* findEOF() const {}
        //const char* findEOL() const {}
        //const char* findEOL(const char* start) const {}
        //StringPiece toStringPiece() const { }


    private:
        char* begin() { return &*m_buffer.begin(); }
        //const char* begin() const { return &*m_buffer.begin(); }
        void makeSpace(size_t lenght);

    private:
        std::vector<char>       m_buffer;
        size_t                  m_readIndex;
        size_t                  m_writeIndex;
    };
}

#endif
