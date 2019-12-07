#include "config.hh"
#include "log.hh"
#include <yaml-cpp/yaml.h>

sylar::ConfigVar<int>::ptr g_int_value_config =
        sylar::Config::Lookup("system.port", (int)8080, "system port");
sylar::ConfigVar<float>::ptr g_float_value_config =
        sylar::Config::Lookup("system.value", (float)8080.0, "system value"); // Just like ats, every config is a obj

void print_yaml(const YAML::Node& node) { // yaml 0.6.3
    if (node.IsScalar()) {
        //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << node.Scalar();
    } else if (node.IsMap()) {
        /*
        for (const auto&i : node) { //TODO
            //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << i.first << " " << i.second;
        }
         */
    } else if (node.IsSequence()) {
        /*
        for (const auto &i : node) {
            //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << i.first << " " << i.second;
        }
         */
    }
}

void test_yaml() {
    YAML::Node root = YAML::LoadFile("/root/CLionProjects/my_sylar/tests/log.yml");
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << root;
    print_yaml(root);
}



int main(int argc, char** argv) {
    sylar::Logger::ptr logger = sylar::Logger::getLoggerInstance();
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));

    test_yaml();

    return 0;
}