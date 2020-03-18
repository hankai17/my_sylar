#include "ns/ns_protocol.hh"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>

namespace sylar {
    namespace ns {
        uint64_t NSNode::GetID(const std::string& ip, uint16_t port) {
            in_addr_t ip_addr = inet_addr(ip.c_str()); // to net int
            uint64_t v = (((uint64_t)ip_addr) << 32) | port;
            return v;
        }

        NSNode::NSNode(const std::string& ip, uint16_t port, uint32_t weight)
        : m_ip(ip),
        m_port(port),
        m_weight(weight) {
            m_id = GetID(ip, port);
        }

        std::ostream& NSNode::dump(std::ostream& os, const std::string& prefix) {
            os << prefix << " NSNode id: " << m_id << " ip: " << m_ip
            << " port: " << m_port << " weight: " << m_weight;
            return os;
        }

        std::string NSNode::toString(const std::string& prefix) {
            std::stringstream ss;
            dump(ss, prefix);
            return ss.str();
        }

        NSNodeSet::NSNodeSet(uint32_t cmd)
        : m_cmd(cmd) {
        }

        void NSNodeSet::add(NSNode::ptr info) {
            sylar::RWMutex::WriteLock lock(m_mutex);
            m_datas[info->getId()] = info;
        }

        NSNode::ptr NSNodeSet::get(uint64_t id) {
            sylar::RWMutex::ReadLock lock(m_mutex);
            auto it = m_datas.find(id);
            return it == m_datas.end() ? nullptr : it->second;
        }

        NSNode::ptr NSNodeSet::del(uint64_t id) {
            NSNode::ptr node;
            sylar::RWMutex::WriteLock lock(m_mutex);
            auto it = m_datas.find(id);
            if (it != m_datas.end()) {
                node = it->second;
                m_datas.erase(it);
            }
            return node;
        }

        void NSNodeSet::listAll(std::vector<NSNode::ptr>& infos) {
            sylar::RWMutex::ReadLock lock(m_mutex);
            for (const auto& i : m_datas) {
                infos.push_back(i.second);
            }
        }

        std::ostream& NSNodeSet::dump(std::ostream& os, const std::string& prefix) {
             os << prefix << "NSNodeSet cmd: " << m_cmd;
             sylar::RWMutex::ReadLock lock(m_mutex);
             os << " size: " << m_datas.size();
             for (const auto& i : m_datas) {
                 i.second->dump(os, prefix + "  ") << std::endl;
             }
             return os;
        }

        std::string NSNodeSet::toString(const std::string& prefix) {
            std::stringstream ss;
            dump(ss, prefix);
            return ss.str();
        }

        size_t NSNodeSet::size() {
            sylar::RWMutex::ReadLock lock(m_mutex);
            return m_datas.size();
        }

        void NSDomain::add(NSNodeSet::ptr infos) {
            sylar::RWMutex::WriteLock lock(m_mutex);
            m_datas[infos->getCmd()] = infos;
        }

        NSNodeSet::ptr NSDomain::get(uint32_t cmd) {
            sylar::RWMutex::ReadLock lock(m_mutex);
            auto it = m_datas.find(cmd);
            return it == m_datas.end() ? nullptr : it->second;
        }

        void NSDomain::add(uint32_t cmd, NSNode::ptr info) {
            auto ns = get(cmd);
            if (!ns) {
                ns.reset(new NSNodeSet(cmd));
                add(ns);
            }
            ns->add(info);
        }

        void NSDomain::del(uint32_t cmd) {
            sylar::RWMutex::WriteLock lock(m_mutex);
            m_datas.erase(cmd);
        }

        NSNode::ptr NSDomain::del(uint32_t cmd, uint64_t id) {
            auto ns = get(cmd);
            if (!ns) {
                return nullptr;
            }
            auto it = ns->del(id);
            if (!ns->size()) {
                del(cmd);
            }
            return it;
        }

        void NSDomain::listAll(std::vector<NSNodeSet::ptr>& infos) {
            sylar::RWMutex::ReadLock lock(m_mutex);
            for (const auto& i : m_datas) {
                infos.push_back(i.second);
            }
        }

        std::ostream& NSDomain::dump(std::ostream& os, const std::string& prefix) {
            os << prefix << " NSDomain domain: " << m_domain;
            sylar::RWMutex::ReadLock lock(m_mutex);
            os << " size: " << m_datas.size() << std::endl;
            for (const auto& i : m_datas) {
                i.second->dump(os, prefix + " ") << std::endl;
            }
            return os;
        }

        std::string NSDomain::toString(const std::string& prefix) {
            std::stringstream ss;
            dump(ss, prefix);
            return ss.str();
        }

        size_t NSDomain::size() {
            sylar::RWMutex::ReadLock lock(m_mutex);
            return m_datas.size();
        }

        void NSDomainSet::add(NSDomain::ptr info) {
            sylar::RWMutex::WriteLock lock(m_mutex);
            m_datas[info->getDomain()] = info;
        }

        void NSDomainSet::del(const std::string& domain) {
            sylar::RWMutex::WriteLock lock(m_mutex);
            m_datas.erase(domain);
        }

        NSDomain::ptr NSDomainSet::get(const std::string& domain, bool auto_create) {
            {
                sylar::RWMutex::ReadLock lock(m_mutex);
                auto it = m_datas.find(domain);
                if (!auto_create) {
                    return it == m_datas.end() ? nullptr : it->second;
                }
            }
            sylar::RWMutex::WriteLock lock(m_mutex);
            auto it = m_datas.find(domain);
            if (it != m_datas.end()) {
                return it->second;
            }
            NSDomain::ptr d(new NSDomain(domain));
            m_datas[domain] = d;
            return d;
        }

        void NSDomainSet::del(const std::string& domain, uint32_t cmd, uint64_t id) {
            auto d = get(domain);
            if (!d) {
                return;
            }
            auto ns = d->get(cmd);
            if (!ns) {
                return;
            }
            ns->del(id);
        }

        void NSDomainSet::listAll(std::vector<NSDomain::ptr>& infos) {
            sylar::RWMutex::ReadLock lock(m_mutex);
            for (const auto& i : m_datas) {
                infos.push_back(i.second);
            }
        }

        std::ostream& NSDomainSet::dump(std::ostream& os, const std::string& prefix) {
            sylar::RWMutex::ReadLock lock(m_mutex);
            os << prefix << "NSDomainSet size: " << m_datas.size();
            for (const auto& i : m_datas) {
                os << prefix;
                i.second->dump(os, prefix + " ") << std::endl;
            }
            return os;
        }

        std::string NSDomainSet::toString(const std::string& prefix) {
            std::stringstream ss;
            dump(ss, prefix);
            return ss.str();
        }

        void NSDomainSet::swap(NSDomainSet& ds) {
            if (this == &ds) {
                return;
            }
            sylar::RWMutex::WriteLock lock(m_mutex);
            sylar::RWMutex::WriteLock lock1(ds.m_mutex);
            m_datas.swap(ds.m_datas);
        }
    }
}