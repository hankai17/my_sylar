#ifndef __ADDRESS_HH__
#define __ADDRESS_HH__

#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <map>

namespace sylar {
    class Address {
    public:
        typedef std::shared_ptr<Address> ptr;
        static Address::ptr Create(const sockaddr* addr, socklen_t addrlen); // sockaddr more normal. normal menas it contains ipv46

        virtual const sockaddr* getAddr() const = 0; // sockaddr more normal
        virtual socklen_t getAddrLen() const = 0;
        virtual std::ostream& insert(std::ostream& os) const = 0;

        std::string toString();
        int getFamily() const;

        static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                int family = AF_UNSPEC, int type = 0, int protocol = 0);
        static bool GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t> >& result,
                int family = AF_UNSPEC);
        static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> >& result,
                const std::string& iface, int family = AF_UNSPEC);
    };

    class IPAddress : public Address {
    public:
        typedef std::shared_ptr<IPAddress> ptr;
        static IPAddress::ptr Create(const char* address, uint16_t port = 0);

        virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
        virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
        virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;
        virtual uint16_t getPort() const = 0;
        virtual void setPort(uint16_t port) = 0;
    };

    class IPv4Address : public IPAddress {
    public:
        typedef std::shared_ptr<IPv4Address> ptr;
        static IPv4Address::ptr Create(const char* address, uint16_t port = 0); // expose

        IPv4Address(const sockaddr_in& address);
        IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

        const sockaddr* getAddr() const override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;

        IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
        IPAddress::ptr networkAddress(uint32_t prefix_len) override;
        IPAddress::ptr subnetMask(uint32_t prefix_len) override;
        uint16_t getPort() const override;
        void setPort(uint16_t port) override;
    private:
        sockaddr_in     m_addr;
    };

    class IPv6Address : public IPAddress {
    public:
        typedef std::shared_ptr<IPv6Address> ptr;
        static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

        IPv6Address(); // Must have default other than "IPv4Address(uint32_t address = INADDR_ANY, uint32_t port = 0);"
        IPv6Address(const sockaddr_in6& address);
        IPv6Address(uint8_t address[16], uint16_t port = 0);

        const sockaddr* getAddr() const override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;

        IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
        IPAddress::ptr networkAddress(uint32_t prefix_len) override;
        IPAddress::ptr subnetMask(uint32_t prefix_len) override;
        uint16_t getPort() const override;
        void setPort(uint16_t port) override;
    private:
        sockaddr_in6     m_addr;
    };

    class UnknownAddress : public Address {
    public:
        typedef std::shared_ptr<UnknownAddress> ptr;
        UnknownAddress(int family);
        UnknownAddress(const sockaddr& address);

        const sockaddr* getAddr() const override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;

    private:
        sockaddr m_addr;
    };
}

#endif
