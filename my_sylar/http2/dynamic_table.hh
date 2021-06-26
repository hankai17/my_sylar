#ifndef __DYNAMIC_TABLE_HH__
#define __DYNAMIC_TABLE_HH__

#include <vector>
#include <string>

namespace sylar {
namespace http2 {

class DynamicTable {
public:
    DynamicTable();
    int32_t update(const std::string& name, const std::string& value); // 先从内定的61个队组中找 找不到则找m_datas
    int32_t findIndex(const std::string& name) const;
    std::pair<int32_t, bool> findPair(const std::string& name, const std::string& value) const;
    std::pair<std::string, std::string> getPair(uint32_t idx) const;
    std::string getName(uint32_t idx) const;
    std::string toString() const;
    void setMaxDataSize(int32_t v) { m_maxDataSize = v; }

public:
    static std::pair<std::string, std::string> GetStaticHeaders(uint32_t idx); // 内定的61个idx
    static int32_t GetStaticHeadersIndex(const std::string& name); // 从内定的61个对组中找
    static std::pair<int32_t, bool> GetStaticHeadersPair(const std::string& name, const std::string& val);
private:
    int32_t m_maxDataSize; // 动态表大小由Setting header table size设置帧定义
    int32_t m_dataSize;
    std::vector<std::pair<std::string, std::string> > m_datas;
};

}
}

#endif
