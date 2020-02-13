#include "buffer.hh"
#include "macro.hh"
#include "endian.hh"

#include <string.h>
#include <sys/uio.h>

/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size

namespace sylar {


    Buffer::Buffer(size_t initialSize)
    : m_buffer(sylar::Buffer::kCheapPrepend + initialSize),
    m_readIndex(sylar::Buffer::kCheapPrepend),
    m_writeIndex(sylar::Buffer::kCheapPrepend) {
        SYLAR_ASSERT(readableBytes() == 0);
        SYLAR_ASSERT(writableBytes() == initialSize);
        SYLAR_ASSERT(prependableBytes() == kCheapPrepend);
    }

    void Buffer::makeSpace(size_t length) {
        if (writableBytes() + prependableBytes() < length + kCheapPrepend) {
            m_buffer.resize(m_writeIndex + length);
        } else { // 如果可读的数据在中间 就重置一下 把数据移到头部
            SYLAR_ASSERT(kCheapPrepend < m_readIndex);
            size_t readable = readableBytes();
            std::copy(begin() + m_readIndex, // begin
                    begin() + m_writeIndex, // end
                    begin() + kCheapPrepend); // dst
            m_readIndex = kCheapPrepend;
            m_writeIndex = m_readIndex + readable;
            SYLAR_ASSERT(readable == readableBytes());
        }
    }

    void Buffer::hasWritten(size_t length) {
        SYLAR_ASSERT(length <= writableBytes());
        m_writeIndex += length;
    }

    void Buffer::ensuerWritableBytes(size_t length) {
        if (writableBytes() < length) {
            makeSpace(length);
        }
        SYLAR_ASSERT(writableBytes() >= length);
    }

    void Buffer::append(const char* data, size_t length) { // 0
        ensuerWritableBytes(length);
        std::copy(data,
                data + length,
                begin() + m_writeIndex);
        hasWritten(length);
    }

    void Buffer::appendInt64(int64_t x) {
        int64_t be64 = sylar::byteswapOnLittleEndian<int64_t>(x);
        append(&be64, sizeof(be64));
    }

    void Buffer::appendInt32(int32_t x) {
        int32_t be32 = sylar::byteswapOnLittleEndian<int32_t>(x);
        append(&be32, sizeof(be32));
    }

    void Buffer::appendInt16(int16_t x) {
        int16_t be16 = sylar::byteswapOnLittleEndian<int16_t>(x);
        append(&be16, sizeof(be16));
    }

    void Buffer::appendInt8(int8_t x) {
        append(&x, sizeof(int8_t));
    }

    void Buffer::retrieve(size_t length) { // 1
        SYLAR_ASSERT(length <= readableBytes());
        if (length < readableBytes()) {
            m_readIndex += length;
        } else {
            retrieveAll();
        }
    }

    void Buffer::retrieveUntil(const char* end) {
        SYLAR_ASSERT(begin() + m_readIndex <= end);
        SYLAR_ASSERT(end <= begin() + m_writeIndex);
        retrieve(end - (begin() + m_readIndex));
    }

    std::string Buffer::retrieveAsString(size_t length) {
        SYLAR_ASSERT(length <= readableBytes());
        std::string result(begin() + m_readIndex, length);
        retrieve(length);
        return result;
    }

    int64_t Buffer::readInt64() {
        SYLAR_ASSERT(readableBytes() >= sizeof(int64_t));
        int64_t be64 = 0;
        ::memcpy(&be64, begin() + m_readIndex, sizeof(be64));
        retrieveInt64();
        return sylar::byteswapOnLittleEndian<int64_t >(be64);
    }

    int32_t Buffer::readInt32() {
        SYLAR_ASSERT(readableBytes() >= sizeof(int32_t));
        int32_t be32 = 0;
        ::memcpy(&be32, begin() + m_readIndex, sizeof(be32));
        retrieveInt32();
        return sylar::byteswapOnLittleEndian<int32_t>(be32);
    }

    int16_t Buffer::readInt16() {
        SYLAR_ASSERT(readableBytes() >= sizeof(int16_t));
        int16_t be16 = 0;
        ::memcpy(&be16, begin() + m_readIndex, sizeof(be16));
        retrieveInt16();
        return sylar::byteswapOnLittleEndian<int16_t>(be16);
    }

    int8_t Buffer::readInt8() {
        SYLAR_ASSERT(readableBytes() >= sizeof(int8_t));
        int8_t be8 = 0;
        ::memcpy(&be8, begin() + m_readIndex, sizeof(be8));
        retrieveInt8();
        return be8;
    }

    void Buffer::prepend(const void* data, size_t length) {
        SYLAR_ASSERT(length <= prependableBytes());
        m_readIndex -= length;
        const char* d = static_cast<const char*>(data);
        std::copy(d, // begin
                d + length, // end
                begin() + m_readIndex); // dst
    }

    void Buffer::prependInt64(int64_t x) {
        int64_t be64 = sylar::byteswapOnLittleEndian<int64_t >(x);
        prepend(&be64, sizeof(be64));
    }

    void Buffer::prependInt32(int32_t x) {
        int32_t be32 = sylar::byteswapOnLittleEndian<int32_t>(x);
        prepend(&be32, sizeof(be32));
    }
    void Buffer::prependInt16(int16_t x) {
        int16_t be16 = sylar::byteswapOnLittleEndian<int16_t>(x);
        prepend(&be16, sizeof(be16));
    }

    void Buffer::prependInt8(int8_t x) {
        prepend(&x, sizeof(x));
    }

    void Buffer::shrink(size_t reserve) {
        Buffer other;
        //other.ensuerWritableBytes(readableBytes() + reserve);
    }

    void Buffer::swap(Buffer& buffer) {
        m_buffer.swap(buffer.m_buffer);
        std::swap(m_readIndex, buffer.m_readIndex);
        std::swap(m_writeIndex, buffer.m_writeIndex);
    }

    ssize_t Buffer::readFd(int fd, int* savedErrno) {
        char extrabuf[65535] = {0};
        struct iovec vec[2];
        const size_t writable = writableBytes();

        vec[0].iov_base = begin() + m_writeIndex;
        vec[0].iov_len = writable;
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof(extrabuf);

        const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
        const ssize_t n = ::readv(fd, vec, iovcnt);
        if (n < 0) {
            *savedErrno = errno;
        //} else if (implicit_cast<size_t>(n) <= writable) {
        } else if (static_cast<size_t>(n) <= writable) {
            m_writeIndex += n;
        } else {
            m_writeIndex = m_buffer.size();
            append(extrabuf, n - writable);
        }
        return n;
    }

}