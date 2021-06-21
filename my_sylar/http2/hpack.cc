#include "hpack.hh"
#include "my_sylar/log.hh"
#include "huffman.hh"

namespace sylar {
namespace http2 {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static std::vector<std::string> s_index_type_strings = {
    "INDEXED",
    "WITH_INDEXING_INDEXED_NAME",
    "WITH_INDEXING_NEW_NAME",
    "WITHOUT_INDEXING_INDEXED_NAME",
    "WITHOUT_INDEXING_NEW_NAME",
    "NERVER_INDEXED_INDEXED_NAME",
    "NERVER_INDEXED_NEW_NAME",
    "ERROR"
};


std::string IndexTypeToString(IndexType type) {
    uint8_t v = (uint8_t)type;
    if(v <= 8) {
        return s_index_type_strings[v];
    }
    return "UNKNOW(" + std::to_string((uint32_t)v) + ")";
}

std::string HeaderField::toString() const {
    std::stringstream ss;
    ss << "[header type=" << IndexTypeToString(type)
       << " h_name=" << h_name
       << " h_value=" << h_value
       << " index=" << index
       << " name=" << name
       << " value=" << value
       << "]";
    return ss.str();
}

int HPack::WriteVarInt(ByteArray::ptr ba, int32_t prefix, uint64_t value, uint8_t flags) {
    size_t pos = ba->getPosition();
    uint64_t v = (1 << prefix) - 1;
    if(value < v) {
        ba->writeFuint8(value | flags);
        return 1;
    }
    ba->writeFuint8(v | flags);
    value -= v;
    while(value >= 128) {
        ba->writeFuint8((0x8 | (value & 0x7f)));
        value >>= 7;
    }
    ba->writeFuint8(value);
    return ba->getPosition() - pos;
}

uint64_t HPack::ReadVarInt(ByteArray::ptr ba, int32_t prefix) {
    //TODO check prefix in (1, 8)
    uint8_t b = ba->readFUint8();
    uint8_t v = (1 << prefix) - 1;
    b &= v;
    if(b < v) {
        return b;
    }
    uint64_t iv = b;
    int m = 0;
    do {
        b = ba->readFUint8();
        iv += ((uint64_t)(b & 0x7F)) << m;
        m += 7;
        //TODO check m >= 63
    } while(b & 0x80);
    return iv;
}

uint64_t HPack::ReadVarInt(ByteArray::ptr ba, uint8_t b, int32_t prefix) { // 取b的后prefix位
    //TODO check prefix in (1, 8)
    uint8_t v = (1 << prefix) - 1;
    b &= v;
    if(b < v) {
        return b;
    }
    uint64_t iv = b;
    int m = 0;
    do {
        b = ba->readFUint8();
        iv += ((uint64_t)(b & 0x7F)) << m;
        m += 7;
        //TODO check m >= 63
    } while(b & 0x80);
    return iv;
}

std::string HPack::ReadString(ByteArray::ptr ba) {
    uint8_t type = ba->readFUint8(); // 首位是1 代表huffman编码
    int len = ReadVarInt(ba, type, 7); // 后7位为 huffman编码的长度
    std::string data;
    data.resize(len);
    ba->read(&data[0], len); // TODO if read failed
    if(type & 0x80) {
        std::string out;
        Huffman::DecodeString(data, out);
        return out;
    }
    return data;
}

int HPack::WriteString(ByteArray::ptr ba, const std::string& str, bool h) {
    int pos = ba->getPosition();
    if(h) {
        std::string new_str;
        Huffman::EncodeString(str, new_str, 0);
        WriteVarInt(ba, 7, new_str.length(), 0x80);
        ba->write(new_str.c_str(), new_str.length());
    } else {
        WriteVarInt(ba, 7, str.length(), 0);
        ba->write(str.c_str(), str.length());
    }
    return ba->getPosition() - pos;
}

HPack::HPack(DynamicTable& table) :
    m_table(table) {
}

int HPack::parse(const std::string& data) {
    ByteArray::ptr ba(new ByteArray(&data[0], data.size(), false));
    return parse(ba, data.size());
}

int HPack::parse(ByteArray::ptr ba, int length) {
    int parsed = 0;
    int pos = ba->getPosition();
    while (parsed < length) {
        HeaderField header; // <名称, 值>
        uint8_t type = ba->readFUint8();
        if(type & 0x80) { // 首位为1 名称与值均在索引表
            uint32_t idx = ReadVarInt(ba, type, 7);
            header.type = IndexType::INDEXED;
            header.index = idx;
        } else {
            if (type & 0x40) { // 前两位为01 值需要编码且增至动态表中
                uint32_t idx = ReadVarInt(ba, type, 6);
                header.type = idx > 0 ? IndexType::WITH_INDEXING_INDEXED_NAME /*名称在表中*/ : IndexType::WITH_INDEXING_NEW_NAME/*名称不在表中 需要编码传递*/;
                header.index = idx;
            } else if((type & 0xF0) == 0) { // 前四位为0  无需曾至表中
                uint32_t idx = ReadVarInt(ba, type, 4);
                header.type = idx > 0 ? IndexType::WITHOUT_INDEXING_INDEXED_NAME : IndexType::WITHOUT_INDEXING_NEW_NAME;
                header.index = idx;
            } else if(type & 0x10) {
                uint32_t idx = ReadVarInt(ba, type, 4);
                header.type = idx > 0 ? IndexType::NERVER_INDEXED_INDEXED_NAME : IndexType::NERVER_INDEXED_NEW_NAME;
                header.index = idx;
            } else {
                return -1;
            }

            if (header.index > 0) {
                header.value = ReadString(ba);
            } else {
                header.name = ReadString(ba);
                header.value = ReadString(ba);
            }
        } // done
        if(header.type == IndexType::INDEXED) {
            auto p = m_table.getPair(header.index);
            header.name = p.first;
            header.value = p.second;
        } else if(header.index > 0) {
            auto p = m_table.getPair(header.index);
            header.name = p.first;
        }
        if(header.type == IndexType::WITH_INDEXING_INDEXED_NAME) {
            m_table.update(m_table.getName(header.index), header.value);
        } else if(header.type == IndexType::WITH_INDEXING_NEW_NAME) {
            m_table.update(header.name, header.value);
        }
        m_headers.emplace_back(std::move(header));
        parsed = ba->getPosition() - pos;
    }
    //for(auto& header : m_headers) {
    //    if(header.type == IndexType::WITH_INDEXING_INDEXED_NAME) {
    //        m_table.update(m_table.getName(header.index), header.value);
    //    } else if(header.type == IndexType::WITH_INDEXING_NEW_NAME) {
    //        m_table.update(header.name, header.value);
    //    }
    //}
    return parsed;

}

int HPack::pack(HeaderField* header, ByteArray::ptr ba) {
    m_headers.push_back(*header);
    int pos = ba->getPosition();

    if(header->type == IndexType::INDEXED) {
        WriteVarInt(ba, 7, header->index, 0x80);
    } else if(header->type == IndexType::WITH_INDEXING_INDEXED_NAME) { // 名称在表中
        WriteVarInt(ba, 6, header->index, 0x40);
        WriteString(ba, header->value, header->h_value);
    } else if(header->type == IndexType::WITH_INDEXING_NEW_NAME) { // 名称不在表中
        WriteVarInt(ba, 6, header->index, 0x40);
        WriteString(ba, header->name, header->h_name);
        WriteString(ba, header->value, header->h_value);
    } else if(header->type == IndexType::WITHOUT_INDEXING_INDEXED_NAME) {
        WriteVarInt(ba, 4, header->index, 0x00);
        WriteString(ba, header->value, header->h_value);
    } else if(header->type == IndexType::WITHOUT_INDEXING_NEW_NAME) {
        WriteVarInt(ba, 4, header->index, 0x00);
        WriteString(ba, header->name, header->h_name);
        WriteString(ba, header->value, header->h_value);
    } else if(header->type == IndexType::NERVER_INDEXED_INDEXED_NAME) {
        WriteVarInt(ba, 4, header->index, 0x10);
        WriteString(ba, header->value, header->h_value);
    } else if(header->type == IndexType::NERVER_INDEXED_NEW_NAME) {
        WriteVarInt(ba, 4, header->index, 0x10);
        WriteString(ba, header->name, header->h_name);
        WriteString(ba, header->value, header->h_value);
    }
    return ba->getPosition() - pos;
}

int HPack::pack(const std::vector<std::pair<std::string, std::string> >& headers, ByteArray::ptr ba) {
    int rt = 0;
    for(auto& i : headers) {
        HeaderField h;
        auto p = m_table.findPair(i.first, i.second);
        if(p.second) {
            h.type = IndexType::INDEXED;
            h.index = p.first;
        } else if(p.first > 0) { // 名在表中 值不在
            h.type = IndexType::WITH_INDEXING_INDEXED_NAME;
            h.index = p.first;
            h.h_value = Huffman::ShouldEncode(i.second);
            h.name = i.first;
            h.value = i.second;
            m_table.update(h.name, h.value); // 更新本地表
        } else { // 名不在表中
            h.type = IndexType::WITH_INDEXING_NEW_NAME;
            h.index = 0;
            h.h_name = Huffman::ShouldEncode(i.first);
            h.name = i.first;
            h.h_value = Huffman::ShouldEncode(i.second);
            h.value = i.second;
            m_table.update(h.name, h.value); // 更新本地表
        }

        rt += pack(&h, ba);
    }
    return rt;
}

}
}
