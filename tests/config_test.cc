#include "config.hh"
#include "log.hh"
#include "application.hh"
#include <yaml-cpp/yaml.h>
#include <vector>
#include <iostream>

sylar::ConfigVar<int>::ptr g_int_value_config =
        sylar::Config::Lookup("system.port", (int)8080, "system port");
sylar::ConfigVar<float>::ptr g_float_value_config =
        sylar::Config::Lookup("system.value", (float)8080.0, "system value"); // Just like ats, every config is a obj
sylar::ConfigVar<std::vector<int> >::ptr g_vec_value_config =
        sylar::Config::Lookup("system.int_vec", std::vector<int> {1, 2, 5}, "vector value");
sylar::ConfigVar<std::list<int> >::ptr g_lst_value_config =
        sylar::Config::Lookup("system.int_lst", std::list<int> {1, 2, 5}, "list value");
sylar::ConfigVar<std::set<int> >::ptr g_set_value_config =
        sylar::Config::Lookup("system.int_set", std::set<int> {1, 2, 5, 1}, "set value");
sylar::ConfigVar<std::map<std::string, int> >::ptr g_map_value_config =
        sylar::Config::Lookup("system.int_map", std::map<std::string, int> {{"1",1}, {"2",2}, {"3",3}}, "map value");
sylar::ConfigVar<std::map<std::string, std::vector<int>> >::ptr g_map_value_config1 =
        sylar::Config::Lookup("system.int_map1", std::map<std::string, std::vector<int> > {{"1",{1}}, {"2",{2,2}}, {"3",{3,3,3}}}, "map value");
sylar::ConfigVar<std::unordered_map<std::string, int> >::ptr g_unor_map_value_config =
        sylar::Config::Lookup("system.int_unor_map", std::unordered_map<std::string, int> {{"1",1}, {"2",2}, {"3",3}}, "unormap value");

void print_yaml(const YAML::Node& node) { // yaml 0.6.3
    if (node.IsScalar()) {
        SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "scalar--->" << node.Scalar();
    } else if (node.IsMap()) {
        for (const auto&it : node) {
            SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "maps--->" << it.first; // << " " << it.second << "--->in maps";
            print_yaml(it.second);
        }
    } else if (node.IsSequence()) {
        for (const auto &it : node) {
            SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "seqs--->" << it.first;
            print_yaml(it);
        }
    } else if (node.IsNull()) {
        SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "null--->";
    }
}

void test_yaml() {
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/log.yml");
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << root;
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "-------------------------";
    print_yaml(root);
}

#define XX(g_var, name, prefix) \
{ \
    const auto& v = g_var->getValue(); \
    for (const auto& i : v) { \
        SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " reverse m_val: " << i; \
    } \
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " toString(): " << g_var->toString(); \
}

#define XXM(g_var, name, prefix) \
{ \
    const auto& v = g_var->getValue(); \
    for (const auto& i : v) { \
        SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " reverse m_val: " << i.first << " " << i.second; \
    } \
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " toString(): " << g_var->toString(); \
}

#define XXM1(g_var, name, prefix) \
{ \
    const auto& v = g_var->getValue(); \
    for (const auto& i : v) { \
        for (const auto& j : i.second ) { \
            SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " reverse m_val: " << i.first << " " << j; \
        } \
    } \
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " toString(): " << g_var->toString(); \
}

void test_config1() {
    XX(g_vec_value_config, vec_int, before);
    XX(g_lst_value_config, lst_int, before);
    XX(g_set_value_config, set_int, before);
    XXM(g_map_value_config, map_int, before);
    XXM1(g_map_value_config1, map1_int, before);
    XXM(g_unor_map_value_config, unor_map_int, before);
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/log.yml");
    sylar::Config::loadFromYaml(root);
    XX(g_lst_value_config, lst_int, after);
    XX(g_vec_value_config, vec_int, after);
    XX(g_set_value_config, set_int, after);
    XXM(g_map_value_config, map_int, after);
    XXM1(g_map_value_config1, map1_int, after);
    XXM(g_unor_map_value_config, unor_map_int, after);
}

void test_config() {
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/log.yml");
    sylar::Config::loadFromYaml(root);
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "after load";
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << g_int_value_config->getValue();
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << g_float_value_config->getValue();
}

namespace sylar {
    class Person {
    public:
        std::string m_name;
        int         m_age;
        bool        m_sex;

        std::string toString() const {
            std::stringstream ss;
            ss << "[Person name=" << m_name
               << " age=" << m_age
               << " sex=" << m_sex
               << "]";
            return ss.str();
        }

        bool operator==(const Person &oth) const {
            return m_name == oth.m_name
                   && m_age == oth.m_age
                   && m_sex == oth.m_sex;
        }
    };

    template <>
    class Lexicalcast<std::string, Person> {
    public:
        Person operator() (const std::string& val) {
           YAML::Node node = YAML::Load(val);
            Person p;
            p.m_name = node["name"].as<std::string>();
            p.m_age  = node["age"].as<int>();
            p.m_sex  = node["sex"].as<bool>();
            return p;
        }
    };

    template<>
    class Lexicalcast<Person, std::string> {
    public:
        std::string operator() (const Person& val) {
            YAML::Node node;
            node["name"] = val.m_name;
            node["age"]  = val.m_age;
            node["sex"]  = val.m_sex;
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
}

sylar::ConfigVar<sylar::Person>::ptr g_person_value_config =
        sylar::Config::Lookup("system.person", sylar::Person(), "system person");
sylar::ConfigVar<std::map<std::string, sylar::Person> >::ptr g_pmap_value_config =
        sylar::Config::Lookup("system.person_map", std::map<std::string, sylar::Person> {}, "system person map");
sylar::ConfigVar<std::map<std::string, std::vector<sylar::Person> > >::ptr g_pmapvec_value_config =
        sylar::Config::Lookup("system.person_map_vec", std::map<std::string, std::vector<sylar::Person> > {}, "system person map vec");

#define XXC(g_var, name, prefix) \
{ \
    const auto& v = g_var->getValue(); \
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " m_val: " << v.toString(); \
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " m_val: " << g_var->toString(); \
}

#define XXCM(g_var, name, prefix) \
{ \
    const auto& v = g_var->getValue(); \
    for (const auto& i : v) { \
        SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " m_val: " << i.first << " " << i.second.toString(); \
    } \
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " m_val: " << g_var->toString(); \
}

#define XXCMV(g_var, name, prefix) \
{ \
    const auto& v = g_var->getValue(); \
    for (const auto& i : v) { \
        for (const auto& j : i.second) { \
            SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " m_val: " << i.first << " " << j.toString(); \
        } \
    } \
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << #prefix " " #name " m_val: " << g_var->toString(); \
}

void test_class() {
    XXC(g_person_value_config, system.person, before);
    XXCM(g_pmap_value_config, system.person_map, before);
    XXCMV(g_pmapvec_value_config, system.person_map_vec, before);
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/log.yml");
    sylar::Config::loadFromYaml(root);
    XXC(g_person_value_config, system.person, after);
    XXCM(g_pmap_value_config, system.person_map, after);
    XXCMV(g_pmapvec_value_config, system.person_map_vec, after);
}

//extern sylar::ConfigVar<std::vector<sylar::LoggerConfig> >::ptr g_log;

void test_log() {
    sylar::ConfigVar<std::vector<sylar::LoggerConfig> >::ptr g_log =
            sylar::Config::Lookup("logs", std::vector<sylar::LoggerConfig>{}, "system log"); // What map's key make up what 'typename T'
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/base_log.yml");
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << root;
    sylar::Config::loadFromYaml(root);
    const auto& items = g_log->getValue(); // vector<LoggerConfig>
    for ( auto i : items) {
        SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << i.toYamlString();
    }
}

void test_loadconfig() {
    sylar::Config::loadFromConfDir("conf");
}

int main(int argc, char** argv) {
    //sylar::Logger::ptr logger = sylar::Logger::getLoggerInstance();
    //logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender)); // That should be replace by a class & this class communicate with config

    //test_yaml();
    //test_config();
    //test_config1();
    //test_class();
    //test_log();

    sylar::Env::getEnvr()->init(argc, argv);
    test_loadconfig();
    sleep(10);
    test_loadconfig();
    return 0;
}

// Premise: Firstly MUST define global ConfigVar
// Secondly read yaml
