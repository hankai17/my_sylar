#ifndef _CONFIG_HH_
#define _CONFIG_HH_

#include <string>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <map>
#include <yaml-cpp/yaml.h>
#include "log.hh"

namespace sylar {
    class ConfigVarBase {
    public:
        typedef std::shared_ptr<ConfigVarBase> ptr;
        ConfigVarBase(const std::string& name, const std::string& desc = "")
                : m_name(name),
                  m_desc(desc) {
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower); // c form; c++ form is std::tolower
        }
        virtual  ~ConfigVarBase() {}
        const std::string& getName() const { return m_name; }
        const std::string& getDesc() const { return m_desc; }

        virtual std::string toString() = 0;
        virtual bool fromString(const std::string& val) = 0;

    protected:
        std::string     m_name;
        std::string     m_desc;
    };

    template <typename T>
    class ConfigVar : public ConfigVarBase {
    public:
        typedef std::shared_ptr<ConfigVar> ptr;
        ConfigVar(const std::string& name, const T& default_val, const std::string& desc = "") // all para after desc must must must have default value
                : ConfigVarBase(name, desc),
                  m_val(default_val) {
        }

        std::string toString() override {
            try {
                return boost::lexical_cast<std::string>(m_val);
            } catch (std::exception& e) {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Configvar toString() exception " << e.what() << " " <<
                                                  typeid(m_val).name();
            }
            return "";
        }

        bool fromString(const std::string& val) override {
            try {
                m_val = boost::lexical_cast<T>(val);
            } catch (std::exception& e) {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Configvar fromeString() exception " << e.what() << " " <<
                                                  typeid(m_val).name();
                return false;
            }
            return true;
        }

        const T& getValue() const { return m_val; } // Why not T
        void setValue(const T& v) { m_val = v; }
    private:
        T      m_val; // not refer
    };

    class Config {
    public:
        typedef std::shared_ptr<Config> ptr;
        typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

        template <typename T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
            std::map<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, std::shared_ptr<sylar::ConfigVarBase>, std::less<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>, std::allocator<std::pair<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, std::shared_ptr<sylar::ConfigVarBase>>>>::iterator it;
            it = m_config.find(name);
            if (it == m_config.end()) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<ConfigVar<T> >(it->second); // dynamic means cast to child
        }

        template <typename T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name, const T& default_val, const std::string& desc = "") { // used as global register
            auto tmp = Lookup<T>(name); // not Lookup(name)
            if (tmp) {
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name: " << name << " exists";
                return tmp;
            }
            if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "name: " << name << " format err";
                throw std::invalid_argument(name); //return nullptr; // Do not return null
            }

            typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_val, desc));
            m_config[name] = v;
            return v;
        }

        static ConfigVarMap& getMap() { return m_config; } // not const

        static void loadFromYaml(const YAML::Node& root);
        static ConfigVarBase::ptr LookupBase(const std::string& name);

    private:
        static ConfigVarMap        m_config; // Why static
    };
}
//#include "config.cc"
#endif