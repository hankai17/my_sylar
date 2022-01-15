#include "mbuffer.hh"
#include "util.hh"
#include "macro.hh"

namespace sylar {

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

    MBuffer::Segment::Segment(size_t length) : // 一个segment里只有一个segment
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

    MBuffer::MBuffer() {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        invariant();
    }

    MBuffer::MBuffer(const MBuffer& copy) {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        copyIn(copy);
    }

    MBuffer::MBuffer(const char* string) {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        copyIn(string, strlen(string));
    }

    MBuffer::MBuffer(const std::string& string) {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
        copyIn(string);
    }

    MBuffer::MBuffer(const void* data, size_t length) {
        m_readAvailable = m_writeAvailable = 0;
        m_writeIt = m_segments.end();
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
                ++m_writeIt;
            }
        }
        SYLAR_ASSERT(length == 0);
        invariant();
    }

    // ???
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

    /*
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
            m_writeIt->writeAvailable() >= readAvailable()) {
            //copyOut(m_writeIt->w)
        }
    }
     */

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

    /*
    iovec MBuffer::writeBuffer(size_t length, bool reallocate) {

    }
     */

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
        for (; it != buffer.m_segments.end(); it++) {
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

    }

    void MBuffer::copyOut(void* buffer, size_t length, size_t pos) const {

    }

    void MBuffer::invariant() const {

    }
}