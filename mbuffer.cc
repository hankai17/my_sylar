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
}