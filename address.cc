#include "address.hh"
#include "log.hh"
#include "endian.hh"

#include <netdb.h>
#include <ifaddrs.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    template <typename T>
    static T CreatMask(uint32_t bits) {
        return (1 << (sizeof(T) * 8 - bits)) - 1;
    }

    template <typename T>
    static uint32_t CountBytes (T value) {
        uint32_t result = 0;
        for (; value; ++result) {
            value &= value - 1;  //二进制数中有多少个1: n & n - 1
        }
        return result;
    }

    Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) { // https://blog.csdn.net/albertsh/article/details/80991684
        if (addr == nullptr) {
            return nullptr;
        }

        Address::ptr result;
        switch (addr->sa_family) {
            case AF_INET: {
                result.reset(new IPv4Address(*(sockaddr_in*)addr));
                break;
            }
            case AF_INET6: {
                result.reset(new IPv6Address(*(sockaddr_in6*)addr));
                break;
            }
            default: {
                result.reset(new UnknownAddress(*addr));
                break;
            }
        }
        return result;
    }

    bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
                       int family, int type, int protocol) {
        addrinfo hints, *results, *next;
        hints.ai_flags = 0;
        hints.ai_family = family;
        hints.ai_socktype = type;
        hints.ai_protocol = protocol;
        hints.ai_addrlen = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        std::string node;
        const char* service_port = NULL;

        if (!host.empty() && host[0] == '[') {
            const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
            if (endipv6) {
                if (*(endipv6 + 1) == ':') {
                    service_port = endipv6 + 2;
                }
                node = host.substr(1, endipv6 - host.c_str() - 1);
            }
        }

        if (node.empty()) { // ipv4
            const char* endipv4 = (const char*)memchr(host.c_str(), ':', host.size());
            if (endipv4) {
                service_port = endipv4 + 1;
            }
            node = host.substr(0, endipv4 ? endipv4 - host.c_str() : host.size());
        }

        int error = getaddrinfo(node.c_str(), service_port, &hints, &results);
        if (error) {
            SYLAR_LOG_ERROR(g_logger) << "Address::Lookup getaddrinfo(" << node.c_str()
                                      << ", " << service_port << "...) err=" << error << " errno=" << errno << " " << gai_strerror(error);
            return false;
        }

        next = results;
        while (next) {
            result.push_back(Create(next->ai_addr, next->ai_addrlen));
            //std::cout << "next->ai_addr: " <<  ((sockaddr_in*)next->ai_addr)->sin_addr.s_addr << std::endl;
            next = next->ai_next;
        }
        freeaddrinfo(results);
        return true;
    }

    bool Address::GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t> >& result,
                                      int family) {
        struct ifaddrs* next, *results;
        if (getifaddrs(&results) != 0) {
            SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses1(result, " << family << ") errno: " << errno
            << " strerrno: " << strerror(errno);
            return false;
        }

        try {
            for (next = results; next; next = next->ifa_next) {
                Address::ptr addr;
                uint32_t prefix_len = ~0u;
                if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                    continue;
                }
                switch (next->ifa_addr->sa_family) {
                    case AF_INET: {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                        uint32_t netmast = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        prefix_len = CountBytes(netmast);
                        break;
                    }
                    case AF_INET6: {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                        in6_addr& netmast = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                        prefix_len = 0;
                        for (int i = 0; i < 16; i++) {
                            prefix_len += CountBytes(netmast.s6_addr[i]);
                        }
                        break;
                    }
                    default:
                        break;
                }
                if (addr) {
                    result.insert(std::make_pair(next->ifa_name,
                            std::make_pair(addr, prefix_len)));
                }
            }
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses1 exception";
            freeifaddrs(results);
            return false;
        }
        freeifaddrs(results);
        return true;
    }

    bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> >& result,
                                      const std::string& iface, int family) {
        //TODO
        return false;
    }

    std::shared_ptr<IPAddress> Address::LookupAnyIPAddress(const std::string& host,
                                                         int family, int type, int proto) {
        std::vector<Address::ptr> result;
        if (Lookup(result, host, family, type, proto)) {
            for (const auto& i : result) {
                IPAddress::ptr addr = std::dynamic_pointer_cast<IPAddress>(i);
                if (addr) {
                    return addr;
                }
            }
        }
        return nullptr;
    }

    IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) { //校验点分ip得到协议 调不同协议的create函数
        addrinfo hints, *results;
        memset(&hints, 0, sizeof(addrinfo));

        hints.ai_flags = AI_NUMERICHOST; // 必须是一个数字地址字符串
        hints.ai_family = AF_UNSPEC;

        int err = getaddrinfo(address, NULL, &hints, &results); // 都是阻塞的 默认解析IPV6和IPV4  如果设只解析IPV4速度则很快
        if (err) {
            SYLAR_LOG_ERROR(g_logger) << "IPAddress::Create(" << address
                                      << ", " << port << ") err=" << err << " errno=" << errno << " " << gai_strerror(err);
            return nullptr;
        }

        try {
            IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress> (
                    Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen)  // Address::Creat Must Must Must static
                    );
            if (result) {
                result->setPort(port);
            }
            freeaddrinfo(results);
            return result;
        } catch (...) {
            freeaddrinfo(results);
            return nullptr;
        }
    }

    std::string Address::toString() {
        std::stringstream ss;
        insert(ss);
        return ss.str();
    }

    int Address::getFamily() const {
        return getAddr()->sa_family;
    }

    IPv4Address::IPv4Address(const sockaddr_in& address) { // Used for Address's switch
        m_addr = address;
        m_addr.sin_family = AF_INET;
    }

    IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = byteswapOnLittleEndian(port);
        m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
    }

    IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) { // Used for caller
        IPv4Address::ptr ipv4(new IPv4Address);
        ipv4->m_addr.sin_port = byteswapOnLittleEndian(port);
        int result = inet_pton(AF_INET, address, &ipv4->m_addr.sin_addr);
        if (result <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", " << port
            << ") result: " << result << " errno: " << errno << " strerrno: " << strerror(errno);
            return nullptr;
        }
        return ipv4;
    }

    const sockaddr* IPv4Address::getAddr() const {
        return (sockaddr*)&m_addr;
    }

    socklen_t IPv4Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream& IPv4Address::insert(std::ostream& os) const  {
        uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr); // resume host order
        os << ((addr >> 24) & 0xFF) << "."
           << ((addr >> 16) & 0xFF) << "."
           << ((addr >> 8)  & 0xFF) << "."
           << (addr & 0xFF);
        os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
        return os;
    }

    IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
        /*
        if (prefix_len > 32) {
            return nullptr;
        }
        sockaddr_in baddr(m_addr);
        baddr.sin_addr.s_addr |= byteswapOnLittleEndian(
                CreatMask<uint32_t >(prefix_len));
        return IPv4Address::ptr(new IPv4Address(baddr));
         */
        return nullptr;
    }

    IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
        //TODO
        return nullptr;
    }

    IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
        //TODO
        return nullptr;
    }

    uint16_t IPv4Address::getPort() const {
        return byteswapOnLittleEndian(m_addr.sin_port);
    }

    void IPv4Address::setPort(uint16_t port) {
        m_addr.sin_port = byteswapOnLittleEndian(port);
        //std::cout<< "after  byteswapOnLittleEndian: " << byteswapOnLittleEndian(port);
    }

    IPv6Address::IPv6Address() {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
    }

    IPv6Address::IPv6Address(const sockaddr_in6& address) {
        m_addr = address;
    }

    IPv6Address::IPv6Address(uint8_t address[16], uint16_t port) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
        m_addr.sin6_port = byteswapOnLittleEndian(port);
        memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
    }

    IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port) {
        IPv6Address::ptr ipv6(new IPv6Address);
        ipv6->m_addr.sin6_port = byteswapOnLittleEndian(port);
        int result = inet_pton(AF_INET6, address, &ipv6->m_addr.sin6_addr);
        if (result <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", "
            << port << ") err: " << result << " errno: " << errno << " strerrno: " << strerror(errno);
            return nullptr;
        }
        return ipv6;
    }

    const sockaddr* IPv6Address::getAddr() const {
        return (sockaddr*)(&m_addr);
    }

    socklen_t IPv6Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream& IPv6Address::insert(std::ostream& os) const {
        os << "[";
        uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
        //bool used_zeros = false; // TODO
        for (size_t i = 0; i < 8; i++) {
            if (i) {
                os << ":";
            }
            os << std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
        }
        os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
        return os;
    }

    IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
        //TODO
        return nullptr;
    }

    IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
        //TODO
        return nullptr;
    }

    IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
        //TODO
        return nullptr;
    }

    uint16_t IPv6Address::getPort() const {
        return byteswapOnLittleEndian(m_addr.sin6_port);
    }

    void IPv6Address::setPort(uint16_t port) {
        m_addr.sin6_port = byteswapOnLittleEndian(port);
    }

    UnknownAddress::UnknownAddress(int family) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sa_family = family;
    }

    UnknownAddress::UnknownAddress(const sockaddr& address) {
        m_addr = address;
    }

    const sockaddr* UnknownAddress::getAddr() const {
        return &m_addr;
    }

    socklen_t UnknownAddress::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream& UnknownAddress::insert(std::ostream& os) const {
        os << "UnknownAddress family: " << m_addr.sa_family;
        return os;
    }

}