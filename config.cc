#include "config.hh"
#include <list>

namespace sylar {

    Config::ConfigVarMap Config::m_config; //!!!

    ConfigVarBase::ptr Config::LookupBase(const std::string& name) { // It just a simple judge
        auto it = Config::getMap().find(name);
        return it == Config::getMap().end() ? nullptr : it->second; // Not use m_data[name]
    }

    static void ListAllMember(const std::string& prefix, const YAML::Node& node,
            std::list<std::pair<std::string, const YAML::Node> >& all_nodes) {
        if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Config invalid name: " <<  prefix;
            return;
        }
        all_nodes.push_back(std::make_pair(prefix, node));
        if (node.IsMap()) {
            for (auto& it : node) { // const auto& it
                    ListAllMember(prefix.empty() ? it.first.Scalar() : prefix + "." + it.first.Scalar(), it.second, all_nodes);
            }
        }
    }

    void Config::loadFromYaml(const YAML::Node& root) {
        std::list<std::pair<std::string, const YAML::Node> > all_nodes;
        ListAllMember("", root, all_nodes);

        for (auto& i : all_nodes) { // for (auto i : all_nodes)
            std::string key = i.first;
            if (key.empty()) {
                continue;
            }
            //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "key: " << key;

            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            ConfigVarBase::ptr var = LookupBase(key);
            if (var) {
                if (i.second.IsScalar()) {
                    var->fromString(i.second.Scalar());
                    //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "key: " << key << " string: " << i.second.Scalar();
                } else {
                    std::stringstream ss;
                    ss << i.second;
                    //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "key: " << key << " string: " << ss.str();
                    var->fromString(ss.str());
                }
            } else {
                //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "not exists this key: " << key << " string: " << i.second;
            }
        }
    }
}