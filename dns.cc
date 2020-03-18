#include "dns.hh"
#include "log.hh"
#include "config.hh"
#include "socket.hh"
#include "util.hh"
#include "iomanager.hh"
#include <set>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    Dns::Dns(const std::string& domain, int type)
    : m_domain(domain),
    m_type(type),
    m_idx(0) {
    }

    static bool check_service_alive(sylar::Address::ptr addr) {
        sylar::Socket::ptr sock = sylar::Socket::CreateTCPSocket();
        return sock->connect(addr, 9527);
    }

    void Dns::init() {
        if (m_type != TYPE_ADDRESS) {
            SYLAR_LOG_ERROR(g_logger) << m_domain << "invalid type: " << m_type;
            return;
        }

        sylar::RWMutex::ReadLock lock(m_mutex);
        auto addrs = m_addrs;
        lock.unlock();
        std::vector<Address::ptr> result;
        for (const auto& i : addrs) {
            if (!sylar::Address::Lookup(result, i, sylar::Socket::IPv4, sylar::Socket::TCP)) {
                SYLAR_LOG_ERROR(g_logger) << m_domain << " invalid address: " << i;
            }
        }
        std::vector<AddressItem> address;
        address.resize(result.size());
        for (size_t i = 0; i < result.size(); i++) {
            address[i].addr = result[i];
            address[i].valid = check_service_alive(result[i]);
        }
        sylar::RWMutex::WriteLock lock1(m_mutex);
        m_address.swap(address);
    }

    void Dns::set(const std::set<std::string>& addrs) {
        {
            sylar::RWMutex::WriteLock lock(m_mutex);
            m_addrs = addrs;
        }
        init();
    }

    sylar::Address::ptr Dns::get(uint32_t seed) {
        if (seed == (uint32_t)-1) {
            seed = sylar::Atomic::addFetch(m_idx, 1);
        }
        sylar::RWMutex::ReadLock lock(m_mutex);
        for (size_t i = 0; i < m_address.size(); i++) {
            auto info = m_address[(seed + i) % m_address.size()];
            if (info.valid) {
                return info.addr;
            }
        }
        return nullptr;
    }

    std::string Dns::toString() {
        std::stringstream ss;
        ss << "Dns domain: " << m_domain << " type: " << m_type
        << " idx: " << m_idx;
        sylar::RWMutex::ReadLock lock(m_mutex);
        ss << " addrs.size: " << m_address.size() << " addrs: ";
        for (const auto& i : m_address) {
            ss << i.addr << ": " << i.valid << ", ";
        }
        lock.unlock();
        return ss.str();
    }

    void Dns::refresh() {
        if (m_type == DOMAIN) {
            std::vector<Address::ptr> result;
            if (!sylar::Address::Lookup(result, m_domain, sylar::Socket::IPv4, sylar::Socket::TCP)) {
                SYLAR_LOG_ERROR(g_logger) << m_domain << " invalid address: " << m_domain;
            }
            std::vector<AddressItem> address;
            address.resize(result.size());
            for (size_t i = 0; i < result.size(); i++) {
                address[i].addr = result[i];
                address[i].valid = check_service_alive(result[i]);
            }
            sylar::RWMutex::WriteLock lock(m_mutex);
            m_address.swap(address);
        } else {
            init();
        }
    }

    struct DnsDefine {
        std::string domain;
        int type;
        std::set<std::string> addrs;
        bool operator==(const DnsDefine& b) const {
            return domain == b.domain
            && type == b.type
            && addrs == b.addrs;
        }

        bool operator<(const DnsDefine& b) const {
            if (domain != b.domain) {
                return domain < b.domain;
            }
            return false;
        }
    };

    DnsManager* DnsManager::dnsmgr = new DnsManager();

    DnsManager* DnsManager::GetInstance() {
        return dnsmgr;
    }

    void DnsManager::init() {
        if (m_refresh) {
            return;
        }
        m_refresh = true;
        sylar::RWMutex::ReadLock lock(m_mutex);
        std::map<std::string, Dns::ptr> dns = m_dns;
        lock.unlock();
        for (const auto& i : dns) {
            i.second->refresh();
        }
        m_refresh = true;
        m_lastUpdateTime = time(0);
    }

    void DnsManager::add(Dns::ptr v) {
        sylar::RWMutex::WriteLock lock(m_mutex);
        m_dns[v->getDomain()] = v;
    }

    Dns::ptr DnsManager::get(const std::string& domain) {
        sylar::RWMutex::WriteLock lock(m_mutex);
        auto it = m_dns.find(domain);
        return it == m_dns.end() ? nullptr : it->second;
    }

    sylar::Address::ptr DnsManager::getAddress(const std::string& service, bool cache, uint32_t seed) {
        auto dns = get(service);
        if (dns) {
            return dns->get(seed);
        }
        if (cache) {
            dns.reset(new Dns(service, Dns::TYPE_DOMAIN));
            add(dns);
        }
        return sylar::Address::LookupAnyIPAddress(service, sylar::Socket::IPv4, sylar::Socket::TCP);
    }

    void DnsManager::start() {
        if (m_timer) {
            return;
        }
        m_timer = sylar::IOManager::GetThis()->addTimer(2000, std::bind(&DnsManager::init, this), true);
    }

    std::ostream& DnsManager::dump(std::ostream& os) {
        sylar::RWMutex::ReadLock lock(m_mutex);
        os << "DnsManager has_timer: " << (m_timer != nullptr)
        << " last_update_time: " << m_lastUpdateTime
        << " dns.size: " << m_dns.size();
        for (const auto& i : m_dns) {
            os << " " << i.second->toString() << std::endl;
        }
        return os;
    }

    template <>
    class Lexicalcast<std::string, DnsDefine> {
    public:
        DnsDefine operator()(const std::string &str) {
            YAML::Node n = YAML::Load(str);
            DnsDefine dd;
            if (!n["domain"].IsDefined()) {
                SYLAR_LOG_ERROR(g_logger) << "dns domain is null";
                return DnsDefine();
            }
            dd.domain = n["domain"].as<std::string>();
            if (!n["type"].IsDefined()) {
                SYLAR_LOG_ERROR(g_logger) << "dns type is null";
                return DnsDefine();
            }
            dd.type = n["type"].as<int>();
            if (n["addrs"].IsDefined()) {
                for (size_t i = 0; i < n["addrs"].size(); i++) {
                    dd.addrs.insert(n["addrs"][i].as<std::string>());
                }
            }
            return dd;
        }
    };

    template <>
    class Lexicalcast<DnsDefine, std::string> {
    public:
        std::string operator() (const DnsDefine& dd) {
            YAML::Node n;
            n["domain"] = dd.domain;
            n["type"] = dd.type;
            for (const auto& i : dd.addrs) {
                n["addrs"].push_back(i);
            }
            std::stringstream ss;
            ss << n;
            return ss.str();
        }
    };

    sylar::ConfigVar<std::set<DnsDefine> >::ptr g_dns_defines =
            sylar::Config::Lookup("dns.config", std::set<DnsDefine>(), "dns config");

    struct DnsIniter {
        DnsIniter() {
            g_dns_defines->addListener("dns.config",[](const std::set<DnsDefine>& old,
                    const std::set<DnsDefine>& new_val) {
                for (auto& n : new_val) {
                    if (n.type == Dns::TYPE_DOMAIN) {
                        Dns::ptr dns(new Dns(n.domain, n.type));
                        dns->refresh();
                        DnsManager::GetInstance()->add(dns);
                    } else if (n.type == Dns::TYPE_ADDRESS) {
                        Dns::ptr dns(new Dns(n.domain, n.type));
                        dns->set(n.addrs);
                        DnsManager::GetInstance()->add(dns);
                    } else {
                        SYLAR_LOG_ERROR(g_logger) << "invalid type: " << n.type
                        << " doamin: " << n.domain;
                    }
                }
            }
            );
        }
    };

    static DnsIniter __dns_init;
}
