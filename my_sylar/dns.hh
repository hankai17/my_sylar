#ifndef __DNS_HH__
#define __DNS_HH__

#include <memory>
#include <set>
#include <vector>
#include <map>
#include "thread.hh"
#include "address.hh"
#include "timer.hh"

namespace sylar {
    class Dns {
    public:
        typedef std::shared_ptr<Dns> ptr;
        enum Type {
            TYPE_DOMAIN = 1,
            TYPE_ADDRESS = 2
        };

        Dns(const std::string& domain, int type);
        void set(const std::set<std::string>& addrs);
        sylar::Address::ptr get(uint32_t seed = -1);
        const std::string& getDomain() const { return m_domain; }
        int getType() const { return m_type; }
        std::string toString();
        void refresh();

    private:
        struct AddressItem {
            sylar::Address::ptr addr;
            bool    valid = false;
        };
        void init();

        std::string         m_domain;
        int                 m_type;
        int                 m_idx;
        sylar::RWMutex      m_mutex;
        std::vector<AddressItem> m_address;
        std::set<std::string>    m_addrs;
    };

    class DnsManager {
    public:
        typedef std::shared_ptr<DnsManager> ptr;
        void init();
        void add(Dns::ptr v);
        Dns::ptr get(const std::string& domain);
        sylar::Address::ptr getAddress(const std::string& service, bool cache, uint32_t seed = -1);
        void start();
        std::ostream& dump(std::ostream& os);
        static DnsManager* GetInstance();
    private:
        DnsManager() {};
        static DnsManager*                     dnsmgr;

        sylar::RWMutex                  m_mutex;
        std::map<std::string, Dns::ptr> m_dns;
        sylar::Timer::ptr               m_timer;
        bool                            m_refresh = false;
        uint64_t                        m_lastUpdateTime = 0;
    };
}

#endif