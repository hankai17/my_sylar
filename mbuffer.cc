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
        m_array.reset(new uint8_t[length], [](char* ptr) {
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

    void MBuffer::Segment::truncate(size_t length);
    void MBuffer::Segment::extend(size_t length);
    const SegmentData readBuffer() const;
    const SegmentData writeBuffer() const;
    SegmentData writeBuffer();
}