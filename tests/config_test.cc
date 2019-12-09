#include "config.hh"
#include "log.hh"
#include <yaml-cpp/yaml.h>
#include <vector>

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
//sylar::ConfigVar<std::map<std::string, std::vector<int>> >::ptr g_map_value_config1 =
 //       sylar::Config::Lookup("system.int_map1", std::map<std::string, std::vector<int> > {{"1",{1}}, {"2",{2,2}}, {"3",{3,3,3}}}, "map value");
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
    //XXM1(g_map_value_config1, map1_int, before);
    XXM(g_unor_map_value_config, unor_map_int, before);
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/log.yml");
    sylar::Config::loadFromYaml(root);
    XX(g_lst_value_config, lst_int, after);
    XX(g_vec_value_config, vec_int, after);
    XX(g_set_value_config, set_int, after);
    XXM(g_map_value_config, map_int, after);
    //XXM1(g_map_value_config1, map_int, after);
    XXM(g_unor_map_value_config, unor_map_int, after);
}

void test_config() {
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/log.yml");
    sylar::Config::loadFromYaml(root);
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "after load";
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << g_int_value_config->getValue();
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << g_float_value_config->getValue();
}

int main(int argc, char** argv) {
    sylar::Logger::ptr logger = sylar::Logger::getLoggerInstance();
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));
    //test_yaml();
    //test_config();
    test_config1();
    return 0;
}