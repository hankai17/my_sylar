#ifndef _CONFIG_HH_
#define _CONFIG_HH_

#include <string>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <functional>

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

    template <typename F, typename T>
    class Lexicalcast { // gerernal
    public:
        T operator() (const F& v) {
            return boost::lexical_cast<T>(v);
        }
    };

    template <typename T>
    class Lexicalcast<std::string, std::vector<T> > { // below all special lexicalcast
    public:
        std::vector<T> operator() (const std::string& v) { //string ---> vector
            YAML::Node node = YAML::Load(v);
            typename std::vector<T> vec;
            std::stringstream ss;
            for (const auto& i : node) {
                ss.str("");
                ss << i;
                vec.push_back(Lexicalcast<std::string, T>()(ss.str()));
            }
            return vec;
        }
    };

    template <typename T>
    class Lexicalcast<std::vector<T>, std::string> {
    public:
        //std::string operator() (std::vector<T>& v) { !!!!!!!!! fuck
            std::string operator() (const std::vector<T>& v) {
            YAML::Node node;
            for (const auto&i : v) {
                node.push_back(YAML::Load(Lexicalcast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    template <typename T>
    class Lexicalcast<std::string, std::list<T> > {
    public:
        std::list<T> operator() (const std::string& v) {
            YAML::Node node = YAML::Load(v);
            std::stringstream ss;
            typename std::list<T> vec;
            for (const auto& i : node) {
                ss.str("");
                ss << i;
                vec.push_back(Lexicalcast<std::string, T>()(ss.str()));
            }
            return vec;
        }
    };

    template <typename T>
    class Lexicalcast<std::list<T>, std::string> {
    public:
        std::string operator() (const std::list<T>& v) {
            std::stringstream ss;
            YAML::Node node;
            for (const auto& i : v) {
                node.push_back(YAML::Load(Lexicalcast<T, std::string>()(i)));
            }
            ss << node;
            return ss.str();
        }
    };

    template <typename T>
    class Lexicalcast<std::string, std::set<T>> {
    public:
        std::set<T> operator() (const std::string& val) {
            YAML::Node node = YAML::Load(val);
            typename std::set<T> m_set;
            std::stringstream ss;
            for (const auto& i : node) {
                ss.str("");
                ss << i;
                m_set.insert(Lexicalcast<std::string, T>()(ss.str()));
            }
            return m_set;
        }
    };

    template <typename T>
    class Lexicalcast<std::set<T>, std::string> {
    public:
        std::string operator() (const std::set<T>& val) {
            std::stringstream ss;
            YAML::Node node;
            for (const auto& i : val) {
                node.push_back(YAML::Load(Lexicalcast<T, std::string>()(i)));
            }
            ss << node;
            return ss.str();
        }
    };

    template <typename T>
    class Lexicalcast<std::string, std::unordered_set<T>> {
    public:
        std::unordered_set<T> operator() (const std::string& val) {
            YAML::Node node = YAML::Load(val);
            typename std::unordered_set<T> m_set;
            std::stringstream ss;
            for (const auto& i : node) {
                ss.str("");
                ss << i;
                m_set.insert(Lexicalcast<std::string, T>()(ss.str()));
            }
            return m_set;
        }
    };

    template <typename T>
    class Lexicalcast<std::unordered_set<T>, std::string> {
    public:
        std::string operator() (const std::unordered_set<T>& val) {
            std::stringstream ss;
            YAML::Node node;
            for (const auto& i : val) {
                node.push_back(YAML::Load(Lexicalcast<T, std::string>()(i)));
            }
            ss << node;
            return ss.str();
        }
    };

    template <typename T>
    class Lexicalcast<std::string, std::map<std::string, T> > {
    public:
        std::map<std::string, T> operator() (const std::string& val) {
            YAML::Node node = YAML::Load(val);
            typename std::map<std::string, T> m_map;
            std::stringstream ss;
            for (const auto& i : node) {
                ss.str("");
                ss << i.second;
                m_map.insert(std::make_pair(i.first.Scalar(),
                        Lexicalcast<std::string, T>()(ss.str()))); // we should formatter i.second
            }
            return m_map;
        }
    };

    template <typename T>
    class Lexicalcast<std::map<std::string, T>, std::string> {
    public:
        std::string operator() (const std::map<std::string, T>& val) {
            YAML::Node node;
            std::stringstream  ss;
            for (const auto& i : val) {
                //node[i.first] = i.second;
                node[i.first] = YAML::Load(Lexicalcast<T, std::string>()(i.second)); // T must base type. If T is vector it will wrong // fool
            }
            ss << node;
            return ss.str();
        }
    };

    template <typename T>
    class Lexicalcast<std::string, std::unordered_map<std::string, T> > {
    public:
        std::unordered_map<std::string, T> operator() (const std::string& val) {
            YAML::Node node = YAML::Load(val);
            typename std::unordered_map<std::string, T> m_map;
            std::stringstream ss;
            for (const auto& i : node) {
                ss.str("");
                ss << i.second;
                m_map.insert(std::make_pair(i.first.Scalar(),
                                            Lexicalcast<std::string, T>()(ss.str()))); // we should formatter i.second
            }
            return m_map;
        }
    };

    template <typename T>
    class Lexicalcast<std::unordered_map<std::string, T>, std::string> {
    public:
        std::string operator() (const std::unordered_map<std::string, T>& val) {
            YAML::Node node;
            std::stringstream  ss;
            for (const auto& i : val) {
                //node[i.first] = i.second;
                node[i.first] = YAML::Load(Lexicalcast<T, std::string>()(i.second));
            }
            ss << node;
            return ss.str();
        }
    };

    template <typename T, class FromStr = Lexicalcast<std::string, T>,
                class ToStr = Lexicalcast<T, std::string> >
    class ConfigVar : public ConfigVarBase {
    public:
        typedef std::shared_ptr<ConfigVar> ptr;
        ConfigVar(const std::string& name, const T& default_val, const std::string& desc = "") // all para after desc must must must have default value
                : ConfigVarBase(name, desc),
                  m_val(default_val) {
        }

        std::string toString() override {
            try {
                //return boost::lexical_cast<std::string>(m_val);
                return ToStr()(m_val);
            } catch (std::exception& e) {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Configvar toString() exception " << e.what() << " " <<
                                                  typeid(m_val).name();
            }
            return "";
        }

        bool fromString(const std::string& val) override {
            try {
                //m_val = boost::lexical_cast<T>(val);
                m_val = FromStr()(val);
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
            it = getMap().find(name);
            if (it == getMap().end()) {
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
            getMap()[name] = v;
            return v;
        }

        static void loadFromYaml(const YAML::Node& root);
        static ConfigVarBase::ptr LookupBase(const std::string& name);

    private:
        static ConfigVarMap        m_config; // Why static
        static ConfigVarMap& getMap() { return m_config; } // not const
    };
}
//#include "config.cc"
#endif