#ifndef __MBUFFER_HH__
#define __MBUFFER_HH__

#include <memory>
#include <vector>
#include <list>
#include <sys/uio.h>

namespace sylar {

    struct MBuffer {
    private:

        struct SegmentData {
        public:
            friend MBuffer;
            SegmentData();
            SegmentData(size_t length);
            SegmentData(void* buffer, size_t length);

            SegmentData slice(size_t start, size_t length = ~0);
            const SegmentData slice(size_t start, size_t length = ~0) const;
            void extend(size_t length);

            void* getStart() { return m_start; }
            const void* getStart() const { return m_start; }
            size_t getLength() const { return m_length; }

        private:
            void setStart(void* v) { m_start = v; }
            void setLength(size_t v) { m_length = v; }

        private:
            void*           m_start;
            size_t          m_length;
            std::shared_ptr<uint8_t> m_array; // MUST provide custom delete
        };

        struct Segment {
        public:
            friend MBuffer;
            Segment(size_t length);
            Segment(SegmentData seg_data);
            Segment(void* buffer, size_t length);

            size_t readAvailable() const;
            size_t writeAvailable() const;
            size_t length() const;
            void produce(size_t length);
            void consume(size_t length);
            void truncate(size_t length);
            void extend(size_t length);
            const SegmentData readBuffer() const;
            const SegmentData writeBuffer() const;
            SegmentData writeBuffer();

        private:
            size_t          m_writeIndex;
            SegmentData     m_data;
        };

    public:
        MBuffer();
        MBuffer(const MBuffer& copy);
        MBuffer(const char* string);
        MBuffer(const std::string& string);
        MBuffer(const void* string, size_t length);
        MBuffer& operator= (const MBuffer& copy);

        size_t readAvailable() const;
        size_t writeAvailable() const;
        size_t segments() const;
        void adopt(void* buffer, size_t length);
        void reserve(size_t length);
        void compact();
        void clear(bool clearWriteAvailAsWell = true);
        void product(size_t length);
        void consume(size_t length);
        void truncate(size_t length);

        const std::vector<iovec> readBuffers(size_t length = ~0) const;
        const iovec readBuffer(size_t length, bool reallocate) const;
        std::vector<iovec> writeBuffers(size_t length = ~0);
        iovec writeBuffer(size_t length, bool reallocate);

        void copyIn(const MBuffer& buffer, size_t length = ~0, size_t pos = 0);
        void copyIn(const char* buffer);
        void copyIn(const std::string& buffer);
        void copyIn(const void* buffer, size_t length);
        void copyOut(MBuffer& buffer, size_t length, size_t pos = 0) const;
        void copyOut(void* buffer, size_t length, size_t pos = 0) const;

        ptrdiff_t find(char delimiter, size_t length = ~0) const;
        ptrdiff_t find(const string& buffer, size_t length = ~0) const;

        std::string toString() const;
        std::string getDelimited(char delimiter, bool eofIsDelimiter = true,
                bool includeDelimiter = true);
        std::string getDelimited(const std::string& delimiter, bool eofIsDelimiter = true,
                                 bool includeDelimiter = true);
        //void visit()

        bool operator== (const MBuffer& rhs) const;
        bool operator!= (const MBuffer& rhs) const;
        bool operator== (const std::string& rhs) const;
        bool operator!= (const std::string& rhs) const;
        bool operator== (const char* rhs) const;
        bool operator!= (const char* rhs) const;

    private:
        std::list<Segment>          m_segments;
        size_t                      m_readAvailable;
        size_t                      m_writeAvailable;
        std::list<Segment>::iterator m_writeIt;

        int opCmp(const MBuffer& rhs) const;
        int opCmp(const char* buffer, size_t length) const;
        void invariant() const;
    };
}

#endif