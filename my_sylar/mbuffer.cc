#include "mbuffer.hh"
#include "util.hh"
#include "macro.hh"
#include "endian.hh"

#include <iostream>

namespace sylar {

    static uint64_t read_nbytes_as_uint(const uint8_t *buf, uint8_t n) {
        uint64_t value = 0;
        memcpy(&value, buf, n);
        return be64toh(value << (64 - n * 8));
    }

    static void write_uint_as_nbytes(uint64_t value, uint8_t n, 
            uint8_t *buf, size_t *len) {
        value = htobe64(value) >> (64 - n * 8);
        memcpy(buf, reinterpret_cast<uint8_t *>(&value), n);
        *len = n;
    }

    size_t MBuffer::var_size(uint64_t src) {
        uint8_t flag = 0;
        if (src > 4611686018427387903) { // 0011 ...
            return 0;
        } else if (src > 1073741823) {
            flag = 0x03;
        } else if (src > 16383) {
            flag = 0x02;
        } else if (src > 63) {
            flag = 0x01;
        } else {
            flag = 0x00;
        }
        return 1 << flag;
    }

    size_t MBuffer::var_size(const uint8_t *src) {
        return static_cast<size_t>(1 << (src[0] >> 6));
    }

    size_t MBuffer::var_size(const std::string &src) {
        return var_size((uint8_t *)src.c_str());
    }

    int MBuffer::var_encode(uint8_t*dst, size_t dst_len,
            size_t &len, uint64_t src) { // 把uint64_t src拷贝到dst中 传出实际长度
        uint8_t flag = 0;
        if (src > 4611686018427387903) {
            return 1;
        } else if (src > 1073741823) {
            flag = 0x03;
        } else if (src > 16383) {
            flag = 0x02;
        } else if (src > 63) {
            flag = 0x01;
        } else {
            flag = 0x00;
        }
        len = 1 << flag; // 计算这个uint64数占几个字节
        if (len > dst_len) {
            return 1;
        }
        size_t dummy = 0;
        write_uint_as_nbytes(src, len, dst, &dummy);
        dst[0] |= (flag << 6); // 粘包
        return 0;
    }

    int MBuffer::var_encode(uint64_t data) {
        size_t real_len = 0;
        iovec iov = writeBuffer(8, true);
        var_encode((uint8_t *)iov.iov_base, 8, real_len, data);
        product(real_len);
        return 0;
    }

    int MBuffer::var_decode(uint64_t &dst, size_t &len, 
            const uint8_t *src, size_t src_len) { // 把const uint8_t* src分解成uint64_t 传出实际长度
        if (src_len < 1) {
            return -1;
        }
        len = 1 << (src[0] >> 6);
        if (src_len < len) {
            return 1;
        }
        uint8_t buf[8] = {0};
        memcpy(buf, src, len);
        buf[0] &= 0x3f;
        dst = read_nbytes_as_uint(buf, len);
        return 0;
    }

    uint64_t MBuffer::var_decode() {
        uint64_t dst = 0;
        size_t real_len = 0;
        iovec iov = writeBuffer(8, true);
        var_decode(dst, real_len, (const uint8_t *)iov.iov_base, 8);
        consume(real_len);
        return dst;
    }

    uint64_t MBuffer::read_QuicVariableInt(const uint8_t *buf, size_t buf_len) {
        uint64_t dst = 0;
        size_t len = 0;
        var_decode(dst, len, buf, buf_len);
        return dst;
    }

    void MBuffer::write_QuicVariableInt(uint64_t data, uint8_t *buf, size_t *len) {
        var_encode(buf, 8, *len, data);
    }

    MBuffer::SegmentData::SegmentData() :
    m_start(nullptr),
    m_length(0) {
    }

    MBuffer::SegmentData::SegmentData(size_t length) :
    m_length(length) {
        m_array.reset(new uint8_t[length], [](uint8_t* ptr) {
            delete[] ptr;
        });
        setStart(m_array.get());
    }

    MBuffer::SegmentData::SegmentData(void* buffer, size_t length) {
        m_array.reset((uint8_t*)buffer, &nop<uint8_t>);
        setStart(m_array.get());
        setLength(length);
    }

    MBuffer::SegmentData MBuffer::SegmentData::slice(size_t start, size_t length) {
        if (length == (size_t)~0) {
            length = m_length - start;
        }
        SYLAR_ASSERT(m_length >= start);
        SYLAR_ASSERT(m_length >= length + start);

        SegmentData result;
        result.m_array = m_array; // use_count++
        result.m_start = ((uint8_t*)getStart() + start);
        result.m_length = length;
        return result;
    }

    const MBuffer::SegmentData MBuffer::SegmentData::slice(size_t start, size_t length) const {
        if (length == (size_t)~0) {
            length = m_length - start;
        }
        SYLAR_ASSERT(m_length >= start);
        SYLAR_ASSERT(m_length >= length + start);

        SegmentData result;
        result.m_array = m_array; // use_count++
        result.m_start = ((uint8_t*)getStart() + start);
        result.m_length = length;
        return result;
    }

    void MBuffer::SegmentData::extend(size_t length) {
        m_length += length;
    }

    MBuffer::Segment::Segment(size_t length) : // 一个segment里只有一个segmentData
    m_writeIndex(0),
    m_data(length) {
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
    }

    MBuffer::Segment::Segment(SegmentData seg_data) :
    m_writeIndex(seg_data.getLength()),
    m_data(seg_data) {
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
    }

    MBuffer::Segment::Segment(void* buffer, size_t length) :
    m_writeIndex(0),
    m_data(buffer, length) {
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
    }

    // ------data---------^----------free--------
    //                    |
    //                 m_writeIndex

    size_t MBuffer::Segment::readAvailable() const {
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
        return m_writeIndex;
    }

    size_t MBuffer::Segment::writeAvailable() const {
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
        return m_data.getLength() - m_writeIndex;
    }

    size_t MBuffer::Segment::length() const {
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
        return m_data.getLength();
    }

    void MBuffer::Segment::produce(size_t length) {
        SYLAR_ASSERT(length <= writeAvailable());
        m_writeIndex += length;
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
    }

    void MBuffer::Segment::consume(size_t length) {
        SYLAR_ASSERT(length <= readAvailable());
        m_writeIndex -= length;
        m_data = m_data.slice(length);
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
    }

    void MBuffer::Segment::truncate(size_t length) {
        SYLAR_ASSERT(length <= readAvailable());
        SYLAR_ASSERT(m_writeIndex == readAvailable());
        m_writeIndex = length;
        m_data = m_data.slice(0, length);
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
    }

    void MBuffer::Segment::extend(size_t length) {
        m_data.extend(length);
        m_writeIndex += length;
    }

    const MBuffer::SegmentData MBuffer::Segment::readBuffer() const {
        invariant();
        return m_data.slice(0, m_writeIndex);
    }

    const MBuffer::SegmentData MBuffer::Segment::writeBuffer() const {
        invariant();
        return m_data.slice(m_writeIndex);
    }

    MBuffer::SegmentData MBuffer::Segment::writeBuffer() {
        invariant();
        return m_data.slice(m_writeIndex);
    }

    void MBuffer::Segment::invariant() const {
        SYLAR_ASSERT(m_writeIndex <= m_data.m_length);
    }

    // 				m_writeIndex
    //     			       ^
    // 				[------|-]
    // Segment ---> Segment ---> Segment ---> ... ---> Segment
    // |			       |                     |
    // |			       |                     |
    // |---------m_readAvailable-------|--m_writeAvaliable---|
    
    MBuffer::MBuffer() {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        m_endian = 1;
        invariant();
    }

    MBuffer::MBuffer(const MBuffer& copy) {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        m_endian = 1;
        copyIn(copy);
    }

    MBuffer::MBuffer(const char* string) {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        m_endian = 1;
        copyIn(string, strlen(string));
    }

    MBuffer::MBuffer(const std::string& string) {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        m_endian = 1;
        copyIn(string);
    }

    MBuffer::MBuffer(const void* data, size_t length) {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        m_endian = 1;
        copyIn(data, length);
    }

    MBuffer& MBuffer::operator= (const MBuffer& copy) {
        clear();
        copyIn(copy);
        return *this;
    }

    size_t MBuffer::readAvailable() const {
        return m_readAvailable;
    }

    size_t MBuffer::writeAvailable() const {
        return m_writeAvailable;
    }

    size_t MBuffer::segments() const {
        return m_segments.size();
    }

    void MBuffer::adopt(void* buffer, size_t length) {
        invariant();
        Segment newSegment(buffer, length);
        if (readAvailable() == 0) {
            m_segments.push_front(newSegment);
            m_writeIt = m_segments.begin();
        } else {
            m_segments.push_back(newSegment);
            if (m_writeAvailable == 0) {
                m_writeIt = m_segments.end();
                --m_writeIt;
            }
        }
        m_writeAvailable += length;
        invariant();
    }

    void MBuffer::reserve(size_t length) {
        if (writeAvailable() < length) {
            Segment newSegment(length * 2 - writeAvailable());
            if (readAvailable() == 0) {
                m_segments.push_front(newSegment);
                m_writeIt = m_segments.begin();
            } else {
                m_segments.push_back(newSegment);
                if (m_writeAvailable == 0) {
                    m_writeIt = m_segments.end();
                    --m_writeIt;
                }
            }
            m_writeAvailable += newSegment.length();
            invariant();
        }
    }

    void MBuffer::compact() {
        invariant();
        if (m_writeIt != m_segments.end()) {
            if (m_writeIt->readAvailable() > 0) {
                Segment newSegment = Segment(m_writeIt->readBuffer());
                m_segments.insert(m_writeIt, newSegment);
            }
            m_writeIt = m_segments.erase(m_writeIt, m_segments.end());
            m_writeAvailable = 0;
        }
        SYLAR_ASSERT(writeAvailable() == 0);
    }

    void MBuffer::clear(bool clearWriteAvailAsWell) {
        invariant();
        if (clearWriteAvailAsWell) {
            m_readAvailable = m_writeAvailable = 0;
            m_segments.clear();
            m_writeIt = m_segments.end();
        } else {
            m_readAvailable = 0;
            if (m_writeIt != m_segments.end() && m_writeIt->readAvailable()) {
                m_writeIt->consume(m_writeIt->readAvailable());
            }
            m_segments.erase(m_segments.begin(), m_writeIt);
        }
        invariant();
        SYLAR_ASSERT(m_readAvailable == 0);
    }

    void MBuffer::product(size_t length) {
        SYLAR_ASSERT(length <= writeAvailable()) ;
        m_readAvailable += length;
        m_writeAvailable -= length;
        while (length > 0) {
            Segment &segment = *m_writeIt;
            size_t toProduce = std::min(segment.writeAvailable(), length);
            segment.produce(toProduce);
            length -= toProduce;
            if (segment.writeAvailable() == 0) {
                ++m_writeIt;
            }
        }
        SYLAR_ASSERT(length == 0);
        invariant();
    }

    void MBuffer::consume(size_t length) {
        SYLAR_ASSERT(length <= readAvailable());
        m_readAvailable -= length;
        while (length > 0) {
            Segment &segment = *m_segments.begin();
            size_t toConsume = std::min(segment.readAvailable(), length);
            segment.consume(toConsume);
            length -= toConsume;
            if (segment.length() == 0) {
                m_segments.pop_front();
            }
        }
        SYLAR_ASSERT(length == 0);
        invariant();
    }

    void MBuffer::truncate(size_t length) {
        SYLAR_ASSERT(length <= readAvailable());
        if (length == m_readAvailable) {
            return;
        }
        if (m_writeIt != m_segments.end() && m_writeIt->readAvailable() != 0) {
            m_segments.insert(m_writeIt, Segment(m_writeIt->readBuffer()));
            m_writeIt->consume(m_writeIt->readAvailable());
        }
        m_readAvailable = length;
        std::list<Segment>::iterator it;
        for (it = m_segments.begin(); it != m_segments.end() && length > 0; it++) {
            Segment &segment = *it;
            if (length <= segment.readAvailable()){
                segment.truncate(length);
                length = 0;
                ++it;
                break;
            } else {
                length -= segment.readAvailable();
            }
        }
        SYLAR_ASSERT(length == 0);
        while (it != m_segments.end() && it->readAvailable() > 0) {
            SYLAR_ASSERT(it->writeAvailable() == 0);
            it = m_segments.erase(it);
        }
        invariant();
    }

    void MBuffer::truncate_r(size_t pos) {
        if (pos >= readAvailable()) {
            return;
        }
        consume(pos);
        invariant();
    }

    // 这个buffer很怪 它不直接对fd提供读写 只是对外暴露buffer
    const std::vector<iovec> MBuffer::readBuffers(size_t length) const {
        if (length == (size_t)~0) {
            length = readAvailable();
        }
        SYLAR_ASSERT(length <= readAvailable());
        std::vector<iovec> result;
        result.reserve(m_segments.size());
        size_t remaining = length;
        std::list<Segment>::const_iterator it;
        for (it = m_segments.begin(); it != m_segments.end(); it++) {
            size_t toConsume = std::min(it->readAvailable(), remaining);
            SegmentData data = it->readBuffer().slice(0, toConsume);
            iovec iov;
            iov.iov_base = (void *)data.getStart();
            iov.iov_len = data.getLength();
            result.push_back(iov);
            remaining -= toConsume;
            if (remaining == 0) {
                break;
            }
        }
        SYLAR_ASSERT(remaining == 0);
        invariant();
        return result;
    }

    const iovec MBuffer::readBuffer(size_t length, bool reallocate) const {
        iovec result;
        result.iov_base = NULL;
        result.iov_len = 0;
        if (length == (size_t)~0) {
            length = readAvailable();
        }
        SYLAR_ASSERT(length <= readAvailable());
        if (readAvailable() == 0) {
            return result;
        }
        if (m_segments.front().readAvailable() >= length) {
            SegmentData data = m_segments.front().readBuffer().slice(0, length);
            result.iov_base = data.getStart();
            result.iov_len = data.getLength();
            return result;
        }
        if (!reallocate) {
            SegmentData data = m_segments.front().readBuffer();
            result.iov_base = data.getStart();
            result.iov_len = data.getLength();
            return result;
        }
        MBuffer *_this = const_cast<MBuffer*>(this);
        if (m_writeIt != m_segments.end() &&
            m_writeIt->writeAvailable() >= readAvailable()) { // 当前块的可写空间远大于所有块的可读空间
            copyOut(m_writeIt->writeBuffer().getStart(), readAvailable());
            Segment newSegment = Segment(m_writeIt->writeBuffer().slice(0, readAvailable())); // 形成新的一块
            _this->m_segments.clear();
            _this->m_segments.push_back(newSegment);
            _this->m_writeAvailable = 0;
            _this->m_writeIt = _this->m_segments.end();
            invariant();
            SegmentData data = newSegment.readBuffer().slice(0, length);
            result.iov_base = data.getStart();
            result.iov_len = data.getLength();
            return result;
        }
        Segment newSegment = Segment(readAvailable());  // 把所有块合并成一个大块
        copyOut(newSegment.writeBuffer().getStart(), readAvailable());
        newSegment.produce(readAvailable());                        // !!!上层用法
        _this->m_segments.clear();
        _this->m_segments.push_back(newSegment);
        _this->m_writeAvailable = 0;
        _this->m_writeIt = _this->m_segments.end();
        invariant();
        SegmentData data = newSegment.readBuffer().slice(0, length);
        result.iov_base = data.getStart();
        result.iov_len = data.getLength();
        return result;
    }

    // 相当于malloc
    std::vector<iovec> MBuffer::writeBuffers(size_t length) {
        if (length == (size_t)~0) {
            length = writeAvailable();
        }
        reserve(length);
        std::vector<iovec> result;
        result.reserve(m_segments.size());
        size_t remaining = length;
        std::list<Segment>::iterator it = m_writeIt;
        while (remaining > 0) {
            Segment &segment = *it;
            size_t toProduce = std::min(segment.writeAvailable(), remaining);
            SegmentData data = segment.writeBuffer().slice(0, toProduce);
            iovec iov;
            iov.iov_base = data.getStart();
            iov.iov_len = data.getLength();
            result.push_back(iov);
            remaining -= toProduce;
            ++it;
        }
        SYLAR_ASSERT(remaining == 0);
        invariant();
        return result;
    }

    // malloc
    iovec MBuffer::writeBuffer(size_t length, bool reallocate) {
        iovec result;
        result.iov_base = NULL;
        result.iov_len = 0;
        if (length == 0u) {
            return result;
        }
        if (writeAvailable() == 0) {
            reserve(length);
            SYLAR_ASSERT(m_writeIt != m_segments.end());
            SYLAR_ASSERT(m_writeIt->writeAvailable() >= length);
            SegmentData data = m_writeIt->writeBuffer().slice(0, length);
            result.iov_base = data.getStart();
            result.iov_len = data.getLength();
            return result;
        }
        if (writeAvailable() > 0 && m_writeIt->writeAvailable() >= length) {
            SegmentData data = m_writeIt->writeBuffer().slice(0, length);
            result.iov_base = data.getStart();
            result.iov_len = data.getLength();
            return result;
        } 
        if (!reallocate) {
            SegmentData data = m_writeIt->writeBuffer();
            result.iov_base = data.getStart();
            result.iov_len = data.getLength();
            return result;
        }
        compact();
        reserve(length);
        SYLAR_ASSERT(m_writeIt != m_segments.end());
        SYLAR_ASSERT(m_writeIt->writeAvailable() >= length);
        SegmentData data = m_writeIt->writeBuffer().slice(0, length); 
        result.iov_base = data.getStart();
        result.iov_len = data.getLength();
        return result;
    }

    void MBuffer::copyIn(const MBuffer& buffer, size_t length, size_t pos) {
        if (pos > buffer.readAvailable())  {
            SYLAR_ASSERT("positioin out of range");
            return;
        }
        if (length == (size_t)~0) {
            length = buffer.readAvailable() - pos;
        }
        SYLAR_ASSERT(buffer.readAvailable() >= length + pos);
        invariant();
        if (length == 0)
            return;
        if (m_writeIt != m_segments.end() && m_writeIt->readAvailable() != 0) {
            m_segments.insert(m_writeIt, Segment(m_writeIt->readBuffer()));
            m_writeIt->consume(m_writeIt->readAvailable());
            invariant();
        }

        std::list<Segment>::const_iterator it = buffer.m_segments.begin();
        while (pos != 0 && it != buffer.m_segments.end()) {
            if (pos < it->readAvailable()) {
                break;
            }
            pos -= it->readAvailable();
            ++it;
        }
        SYLAR_ASSERT(it != buffer.m_segments.end());
        for (; it != buffer.m_segments.end(); ++it) {
            size_t toConsume = std::min(it->readAvailable() - pos, length);
            if (m_readAvailable != 0 && it == buffer.m_segments.begin()) {
                auto previousIt = m_writeIt;
                --previousIt;
                if ((char*)previousIt->readBuffer().getStart() +
                    previousIt->readBuffer().getLength() == (char *)it->readBuffer().getStart() + pos &&
                    previousIt->m_data.m_array.get() == it->m_data.m_array.get()
                    ) {
                    SYLAR_ASSERT(previousIt->writeAvailable() == 0);
                    previousIt->extend(toConsume);
                    m_readAvailable += toConsume;
                    length -= toConsume;
                    pos = 0;
                    if (length == 0) {
                        break;
                    }
                    continue;
                }
            }
            Segment newSegment = Segment(it->readBuffer().slice(pos, toConsume));
            m_segments.insert(m_writeIt, newSegment);
            m_readAvailable += toConsume;
            length -= toConsume;
            pos = 0;
            if (length == 0) {
                break;
            }
        }
        SYLAR_ASSERT(length == 0);
        SYLAR_ASSERT(readAvailable() >= length);
    }

    void MBuffer::copyIn(const char* buffer) {
        copyIn(buffer, strlen(buffer));
    }

    void MBuffer::copyIn(const std::string& buffer) {
        copyIn(buffer.c_str(), buffer.size());
    }

    void MBuffer::copyIn(const void* buffer, size_t length) {
        invariant();
        while (m_writeIt != m_segments.end() && length > 0) {
            size_t todo = std::min(length, m_writeIt->writeAvailable());
            memcpy(m_writeIt->writeBuffer().getStart(), buffer, todo);
            m_writeIt->produce(todo);
            m_writeAvailable -= todo;
            m_readAvailable += todo;
            buffer = (unsigned char *)buffer + todo;
            length -= todo;
            if (m_writeIt->writeAvailable() == 0) {
                ++m_writeIt;
            }
            invariant();
        }
        if (length > 0) {
            Segment newSegment(length);
            memcpy(newSegment.writeBuffer().getStart(), buffer, length);
            newSegment.produce(length);
            m_segments.push_back(newSegment);
            m_readAvailable += length;
        }
        SYLAR_ASSERT(readAvailable() >= length);
    }

    void MBuffer::copyOut(MBuffer& buffer, size_t length, size_t pos) const {
        buffer.copyIn(*this, length, pos);
    }

    void MBuffer::copyOut(void* buffer, size_t length, size_t pos) const {
        if (length == 0) {
            return;
        }
        SYLAR_ASSERT(length + pos <= readAvailable());
        unsigned char *next = (unsigned char*)buffer;
        std::list<Segment>::const_iterator it = m_segments.begin();
        while (pos != 0 && it != m_segments.end()) {
            if (pos < it->readAvailable()) {
                break;
            }
            pos -= it->readAvailable();
            ++it;
        }
        SYLAR_ASSERT(it != m_segments.end());
        for (; it != m_segments.end(); it++) {
            size_t todo = std::min(length, it->readAvailable() - pos);
            memcpy(next, (char *)it->readBuffer().getStart() + pos, todo);
            next += todo;
            length -= todo;
            pos = 0;
            if (length == 0) {
                break;
            }
        }
        SYLAR_ASSERT(length == 0);
    }

    ptrdiff_t MBuffer::find(char delimiter, size_t length) const {
        if (length == (size_t)~0) {
            length = readAvailable();
        }
        SYLAR_ASSERT(length <= readAvailable());
        size_t totalLength = 0;
        bool success = false;

        std::list<Segment>::const_iterator it;
        for (it = m_segments.begin(); it != m_segments.end(); it++) {
            const void *start = it->readBuffer().getStart();
            size_t toscan = std::min(length, it->readAvailable());
            const void *point = memchr(start, delimiter, toscan);
            if (point != NULL) {
                success = true;
                totalLength += (unsigned char*)point - (unsigned char*)start;
                break;
            }
            totalLength += toscan;
            length -= toscan;
            if (length == 0) {
                break;
            }
        }
        if (success) {
            return totalLength;
        }
        return -1;
    }

    ptrdiff_t MBuffer::find(const std::string& buffer, size_t length) const {
        /*
        if (length == (size_t)~0) {
            length = readAvailable();
        }
        SYLAR_ASSERT(length <= readAvailable());
        SYLAR_ASSERT(!string.empty());

        size_t totalLength = 0;
        size_t foundSoFar = 0;
        std::list<Segment>::const_iterator it;
        for (it = m_segments.begin(); it != m_segments.end(); it++) {
            const void *start = it->readBuffer().start();
            size_t toScan = std::min(length, it->readAvailable());
            while (toScan > 0) {
                if (foundSoFar == 0) {

                }
            }
        }
        */
        return -1;
    }

    std::string MBuffer::toString() const {
        if (m_readAvailable == 0) {
            return std::string();
        }
        std::string result;
        result.resize(m_readAvailable);
        copyOut(&result[0], result.size());
        return result;
    }

    uint8_t* MBuffer::toChar() const { // Only first block
        if (m_readAvailable == 0) {
            return nullptr;
        }
        auto it = m_segments.begin();
        if (it == m_segments.end()) {
            return nullptr;
        }
        return (uint8_t *)it->readBuffer().getStart();
    }

    std::string MBuffer::toHexString() const {
        std::string str = toString();
        std::stringstream ss;
        for(size_t i = 0; i < str.size(); ++i) {
            if(i > 0 && i % 32 == 0) {
                ss << std::endl;
            }
            ss << std::setw(2) << std::setfill('0') << std::hex
               << (int)(uint8_t)str[i] << " ";
        }
        return ss.str();
    }

    std::string MBuffer::getDelimited(char delimiter, bool eofIsDelimiter, 
        bool includeDelimiter) {
        /*
        ptrdiff_t offset = find(delimiter, ~0);
        SYLAR_ASSERT(offset >= -1);
        if (offset == -1 && !eofIsDelimiter) {
            SYLAR_ASEERT("getDelimited failed");
        }
        eofIsDelimiter = offset == -1;
        */
        std::string result;
        return result;
    }

    std::string MBuffer::getDelimited(const std::string& delimiter, bool eofIsDelimiter,
        bool includeDelimiter) {
        std::string result;
        return result;
    }

    void MBuffer::visit(std::function<void (const void*, size_t)> dg, 
        size_t length) const {
        if (length == (size_t)~0) {
            length = readAvailable();
        }
        SYLAR_ASSERT(length <= readAvailable());
        std::list<Segment>::const_iterator it;
        for (it = m_segments.begin(); it != m_segments.end(); it++) {
            size_t todo = std::min(length, it->readAvailable());
            SYLAR_ASSERT(todo != 0);
            dg(it->readBuffer().getStart(), todo);
            length -= todo;
        }
        SYLAR_ASSERT(length == 0);
    }

    void MBuffer::write(const void* buffer, size_t length) {
        return copyIn(buffer, length);
    }

    void MBuffer::read(void* buffer, size_t length) {
        return copyOut(buffer, length, 0);
    }

    void MBuffer::read(void* buffer, size_t length, size_t position) const {
        return copyOut(buffer, length, position);
    }

    void MBuffer::writeFint8(int8_t value) {
        write(&value, sizeof(value));
    }

    void MBuffer::writeFuint8(uint8_t value) {
        write(&value, sizeof(value));
    }

    void MBuffer::writeFint16(int16_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void MBuffer::writeFuint16(uint16_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void MBuffer::writeFuint24(uint32_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(uint8_t) * 3);
    }

    void MBuffer::writeFint32(int32_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void MBuffer::writeFuint32(uint32_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void MBuffer::writeFint64(int64_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void MBuffer::writeFuint64(uint64_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    static uint32_t EncodeZigzag32(const int32_t& v) {
        if (v < 0) {
            return ((uint32_t)(-v)) * 2 - 1;
        } else {
            return v * 2;
        }
    }

    static uint64_t EncodeZigzag64(const int64_t& v) {
        if (v < 0) {
            return ((uint64_t) (-v)) * 2 - 1;
        } else {
            return v * 2;
        }
    }

    static int32_t DecodeZigzag32(const uint32_t& v) {
        return (v >> 1) ^ -(v & 1);
    }

    static int64_t DecodeZigzag64(const uint64_t& v) {
        return (v >> 1) ^ -(v & 1);
    }

    void MBuffer::writeInt32(int32_t value) {
        writeUint32(EncodeZigzag32(value));
    }

    void MBuffer::writeUint32(uint32_t value) {
        uint8_t tmp[5];
        uint8_t i = 0;
        while(value >= 0x80) {
            tmp[i++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i++] = value;
        write(tmp, i);
    }

    void MBuffer::writeInt64(int64_t value) {
        writeUint64(EncodeZigzag64(value));
    }

    void MBuffer::writeUint64(uint64_t value) {
        uint8_t tmp[10];
        uint8_t i = 0;
        while(value >= 0x80) {
            tmp[i++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i++] = value;
        write(tmp, i);
    }

    void MBuffer::writeFloat(float value) {
        uint32_t v;
        memcpy(&v, &value, sizeof(value));
        writeFuint32(v);
    }

    void MBuffer::writeDouble(double value) {
        uint64_t v;
        memcpy(&v, &value, sizeof(value));
        writeFuint64(v);
    }

    void MBuffer::writeStringF16(const std::string& value) {
        writeFuint16(value.size());
        write(value.c_str(), value.size());
    }

    void MBuffer::writeStringF32(const std::string& value) {
        writeFuint32(value.size());
        write(value.c_str(), value.size());
    }

    void MBuffer::writeStringF64(const std::string& value) {
        writeFuint64(value.size());
        write(value.c_str(), value.size());
    }

    void MBuffer::writeStringVint(const std::string& value) {
        writeUint64(value.size());
        write(value.c_str(), value.size());
    }

    void MBuffer::writeStringWithoutLength(const std::string& value) {
        write(value.c_str(), value.size());
    }

    int8_t MBuffer::readFint8() {
        int8_t v;
        read(&v, sizeof(v));
        return v;
    }

    uint8_t MBuffer::readFUint8() {
        uint8_t v;
        read(&v, sizeof(v));
        return v;
    }

#define XX(type) \
    type v; \
    read(&v, sizeof(v)); \
    if(m_endian == SYLAR_BYTE_ORDER) { \
        return v; \
    } else { \
        return byteswap(v); \
    }

    int16_t MBuffer::readFint16() {
        XX(int16_t);
    }

    uint16_t MBuffer::readFUint16() {
        XX(uint16_t);
    }

    uint32_t MBuffer::readFUint24() {
        uint32_t v;
        read(&v, sizeof(uint8_t) * 3);
        if (m_endian == SYLAR_BYTE_ORDER) {
            return v;
        } else {
            return byteswap(v);
        }
    }

    int32_t MBuffer::readFint32() {
        XX(int32_t);
    }

    uint32_t MBuffer::readFUint32() {
        XX(uint32_t);
    }

    int64_t MBuffer::readFint64() {
        XX(int64_t);
    }

    uint64_t MBuffer::readFUint64() {
        XX(uint64_t);
    }
#undef XX

    int32_t MBuffer::readInt32() {
        return DecodeZigzag32(readUint32());
    }

    uint32_t MBuffer::readUint32() {
        uint32_t result = 0;
        for(int i = 0; i < 32; i += 7) {
            uint8_t b = readFUint8();
            if(b < 0x80) {
                result |= ((uint32_t)b) << i;
                break;
            } else {
                result |= (((uint32_t)(b & 0x7f)) << i);
            }
        }
        return result;
    }

    int64_t MBuffer::readInt64() {
        return DecodeZigzag64(readUint64());
    }

    uint64_t MBuffer::readUint64() {
        uint64_t result = 0;
        for(int i = 0; i < 64; i += 7) {
            uint8_t b = readFUint8();
            if(b < 0x80) {
                result |= ((uint64_t)b) << i;
                break;
            } else {
                result |= (((uint64_t)(b & 0x7f)) << i);
            }
        }
        return result;
    }

    float MBuffer::readFloat() {
        uint32_t v = readFUint32();
        float value;
        memcpy(&value, &v, sizeof(v));
        return value;
    }

    double MBuffer::readDouble() {
        uint64_t v = readFUint64();
        double value;
        memcpy(&value, &v, sizeof(v));
        return value;
    }

    std::string MBuffer::readStringF16() {
        uint16_t len = readFUint16();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string MBuffer::readStringF32() {
        uint32_t len = readFUint32();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string MBuffer::readStringF64() {
        uint64_t len = readFUint64();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string MBuffer::readStringVint() {
        uint64_t len = readUint64();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    bool MBuffer::operator== (const MBuffer& rhs) const {
        if (rhs.readAvailable() != readAvailable()) {
            return false;
        }
        return opCmp(rhs) == 0;
    }

    bool MBuffer::operator!= (const MBuffer& rhs) const {
        if (rhs.readAvailable() != readAvailable()) {
            return true;
        }
        return opCmp(rhs) != 0; 
    }

    bool MBuffer::operator== (const std::string& rhs) const {
        if (rhs.size() != readAvailable()) {
            return false;
        }
        return opCmp(rhs.c_str(), rhs.size()) == 0;
    }

    bool MBuffer::operator!= (const std::string& rhs) const {
        if (rhs.size() != readAvailable()) {
            return true;
        }
        return opCmp(rhs.c_str(), rhs.size()) != 0;

    }

    bool MBuffer::operator== (const char* rhs) const {
        size_t length = strlen(rhs);
        if (length != readAvailable()) {
            return false;
        }
        return opCmp(rhs, length) == 0;
    }

    bool MBuffer::operator!= (const char* rhs) const {
        size_t length = strlen(rhs);
        if (length != readAvailable()) {
            return true;
        }
        return opCmp(rhs, length) != 0;
    }

    int MBuffer::opCmp(const MBuffer& rhs) const {
        std::list<Segment>::const_iterator leftIt, rightIt;
        int lengthResult = (int)((ptrdiff_t)readAvailable() - (ptrdiff_t)rhs.readAvailable());
        leftIt = m_segments.begin();
        rightIt = m_segments.begin();
        size_t leftOffset = 0, rightOffset = 0;
        while (leftIt != m_segments.end() && rightIt != rhs.m_segments.end()) {
            SYLAR_ASSERT(leftOffset <= leftIt->readAvailable());
            SYLAR_ASSERT(rightOffset <= rightIt->readAvailable());

            size_t tocompare = std::min(leftIt->readAvailable() - leftOffset,
                rightIt->readAvailable() - rightOffset);
            if (tocompare == 0) {
                break;
            }
            int result = memcmp(
                        (const unsigned char *)leftIt->readBuffer().getStart() + leftOffset,
                        (const unsigned char *)rightIt->readBuffer().getStart() + rightOffset,
                        tocompare);
            if (result != 0) {
                return result;
            }
            leftOffset += tocompare;
            rightOffset += tocompare;
            if (leftOffset == leftIt->readAvailable()) {
                leftOffset = 0;
                ++leftIt;
            }
            if (rightOffset == rightIt->readAvailable()) {
                rightOffset = 0;
                ++rightIt;
            }
        }
        return lengthResult;
    }

    int MBuffer::opCmp(const char* buffer, size_t length) const {
        size_t offset = 0;
        std::list<Segment>::const_iterator it;
        int lengthResult = (int)((ptrdiff_t)readAvailable() - (ptrdiff_t)length);
        if (lengthResult > 0) {
            length = readAvailable();
        }
        for (it = m_segments.begin(); it != m_segments.end(); it++) {
            size_t tocompare = std::min(it->readAvailable(), length);
            int result = memcmp(it->readBuffer().getStart(), buffer + offset, tocompare);
            if (result != 0) {
                return result;
            }
            length -= tocompare;
            offset += tocompare;
            if (length == 0) {
                return lengthResult;
            }
        }
        return lengthResult;
    }

    void MBuffer::invariant() const {
    #if 0
        size_t read = 0;
        size_t write = 0;
        bool seenWrite = false; // 空洞
        std::list<Segment>::const_iterator it;
        for (it = m_segments.begin(); it != m_segments.end(); it++) {
            const Segment &segment = *it;
            SYLAR_ASSERT(!seenWrite || (seenWrite && segment.readAvailable() == 0)); // 确保前面的块是"实心"的
            read += segment.readAvailable();
            write += segment.writeAvailable();
            if (!seenWrite && segment.writeAvailable() != 0) { // 这个块没有满即有剩余空间
                seenWrite = true;
                SYLAR_ASSERT(m_writeIt == it);                  // 不满得这个块是当前块
            }
            std::list<Segment>::const_iterator nextIt = it;
            ++nextIt;
            if (nextIt != m_segments.end()) {
                const Segment& next = *nextIt;
                if (segment.writeAvailable() == 0 &&  // 当前块是满的 且下一块有数据
                    next.readAvailable() != 0) {
                    SYLAR_ASSERT((const unsigned char*)segment.readBuffer().start() +
                        segment.readAvailable() != next.readBuffer().start() ||
                        segment.m_data.m_array.get() != next.m_data.m_array.get()); // ?
                } else if (segment.writeAvailable() != 0 && // 当前块不满 且下一块是空
                    next.readAvailable() == 0) {
                    SYLAR_ASSERT((const unsigned char*)segment.writeBuffer().start() +
                        segment.writeAvailable() != next.writeBuffer().start() ||
                        segement.m_data.m_array.get() != next.m_data.m_array.get());
                }
            }
        }
        SYLAR_ASSERT(read == m_readAvailable);
        SYLAR_ASSERT(write == m_writeAvailable);
        SYLAR_ASSERT(write != 0 || (write == 0 && m_writeIt == m_segments.end()));
    #endif
    }
}
