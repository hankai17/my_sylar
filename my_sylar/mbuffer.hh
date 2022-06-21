#ifndef __MBUFFER_HH__
#define __MBUFFER_HH__

#include <memory>
#include <vector>
#include <list>
#include <functional>
#include <sys/uio.h>
#include <cstddef>

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
            void invariant() const;
        };

    public:
        typedef std::shared_ptr<MBuffer> ptr;

        static size_t var_size(uint64_t src);
        static size_t var_size(const uint8_t *src);
        static size_t var_size(const std::string &src);
        static int var_encode(uint8_t*dst, size_t dst_len, size_t &len, uint64_t src);
        static int var_decode(uint64_t &dst, size_t &len, const uint8_t *src, size_t src_len);
        static uint64_t read_QuicVariableInt(const uint8_t *buf, size_t buf_len);
        static void write_QuicVariableInt(uint64_t data, uint8_t *buf, size_t *len);

        int var_encode(uint64_t data); // data写入segmentData中
        uint64_t var_decode(); // 解segmentData中的数据

        MBuffer();
        MBuffer(const MBuffer& copy);
        MBuffer(const char* string);
        MBuffer(const std::string& string);
        MBuffer(const void* string, size_t length);
        MBuffer& operator= (const MBuffer& copy);
        virtual ~MBuffer() {}

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
        void truncate_r(size_t pos);

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
        ptrdiff_t find(const std::string& buffer, size_t length = ~0) const;

        std::string toString() const;
        uint8_t *toChar() const;
        std::string toHexString() const;
        std::string getDelimited(char delimiter, bool eofIsDelimiter = true,
                bool includeDelimiter = true);
        std::string getDelimited(const std::string& delimiter, bool eofIsDelimiter = true,
                                 bool includeDelimiter = true);
        void visit(std::function<void (const void*, size_t)> dg, size_t length = ~0) const;

        void write(const void* buffer, size_t length);
        void read(void* buffer, size_t length);
        void read(void* buffer, size_t length, size_t position) const;

        void writeFint8(int8_t value);
        void writeFuint8(uint8_t value);
        void writeFint16(int16_t value);
        void writeFuint16(uint16_t value);
        void writeFuint24(uint32_t value);
        void writeFint32(int32_t value);
        void writeFuint32(uint32_t value);
        void writeFint64(int64_t value);
        void writeFuint64(uint64_t value);

        void writeInt32(int32_t value);
        void writeUint32(uint32_t value);
        void writeInt64(int64_t value);
        void writeUint64(uint64_t value);

        void writeFloat(float value);
        void writeDouble(double value);

        void writeStringF16(const std::string& value);
        void writeStringF32(const std::string& value);
        void writeStringF64(const std::string& value);
        void writeStringVint(const std::string& value);
        void writeStringWithoutLength(const std::string& value);

        int8_t readFint8();
        uint8_t readFUint8();
        int16_t readFint16();
        uint16_t readFUint16();
        uint32_t readFUint24();
        int32_t readFint32();
        uint32_t readFUint32();
        int64_t readFint64();
        uint64_t readFUint64();

        int32_t readInt32();
        uint32_t readUint32();
        int64_t readInt64();
        uint64_t readUint64();

        float readFloat();
        double readDouble();

        std::string readStringF16();
        std::string readStringF32();
        std::string readStringF64();
        std::string readStringVint();

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
        int8_t                      m_endian;

        int opCmp(const MBuffer& rhs) const;
        int opCmp(const char* buffer, size_t length) const;
        void invariant() const;
    };

    struct MBuffer_t : public MBuffer {
    public:
        typedef std::shared_ptr<MBuffer_t> ptr;
        MBuffer_t(const MBuffer &buffer, uint64_t now)
            : MBuffer(buffer),
              time(now) {}
        uint64_t time = 0;
    };
}

#endif
