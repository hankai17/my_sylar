#include "bytearray.hh"
#include "endian.hh"

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    ByteArray::Node::Node()
    : ptr(nullptr),
    next(nullptr),
    size(0) {
    }

    ByteArray::Node::Node(size_t s)
    : ptr(new char[s]),
    next(nullptr),
    size(s) {
    }

    ByteArray::Node::~Node() {
        if (ptr) {
            delete [] ptr;
        }
        ptr = nullptr;
        next = nullptr;
        size = 0;
    }

    ByteArray::ByteArray(size_t base_size)
    : m_position(0),
    m_baseSize(base_size),
    m_size(0),
    m_capacity(base_size),
    m_endian(SYLAR_BIG_ENDIAN),
    m_root(new Node(base_size)),
    m_cur(m_root) {
    }

    ByteArray::~ByteArray() {
        Node* tmp = m_root;
        while (tmp) {
            m_cur = tmp;
            tmp = tmp->next;
            delete m_cur;
        }
    }

    bool ByteArray::isLittleEndian() const {
        return m_endian == SYLAR_LITTLE_ENDIAN;
    }

    void ByteArray::setIsLittleEndian(bool val) {
        if (val) {
            m_endian = SYLAR_LITTLE_ENDIAN;
        } else {
            m_endian = SYLAR_BIG_ENDIAN;
        }
    }

    void ByteArray::writeFint8(int8_t value) {
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint8(uint8_t value) {
        write(&value, sizeof(value));
    }

    void ByteArray::writeFint16(int16_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint16(uint16_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFint32(int32_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint32(uint32_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFint64(int64_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint64(uint64_t value) {
        if (m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    static uint32_t EncodeZigzag32(const int32_t& v) {
        if (v < 0) {
            return
        }
    }

    void ByteArray::writeInt32(int32_t value) {
        writeUint32()
    }

    void ByteArray::writeUint32(uint32_t value) {

    }

    void ByteArray::writeInt64(int64_t value) {

    }

    void ByteArray::writeUint64(uint64_t value) {

    }

    void ByteArray::writeFloat(float value) {

    }

    void ByteArray::writeDouble(double value) {

    }

    void ByteArray::writeStringF16(const std::string& value) {

    }

    void ByteArray::writeStringF32(const std::string& value) {

    }

    void ByteArray::writeStringF64(const std::string& value) {

    }

    void ByteArray::writeStringVint(const std::string& value) {

    }

}