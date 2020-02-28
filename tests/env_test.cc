#include "log.hh"
#include "env.hh"
#include <unistd.h>
#include <iostream>
#include <fstream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

struct A {
    A() {
        std::ifstream ifs("/proc/" + std::to_string(getpid()) + "/cmdline", std::ios::binary); // ofstream是从内存到硬盘，ifstream是从硬盘到内存
        std::string content;
        content.resize(4096);
        ifs.read(&content[0], content.size());
        content.resize(ifs.gcount());
        for (size_t i = 0; i < content.size(); i++) {
            SYLAR_LOG_DEBUG(g_logger) << i << " - " << content[i] << " - " << (int)content[i];
        }
        SYLAR_LOG_DEBUG(g_logger) << content;
    }
};

A a;

int main(int argc, char** argv) {
    sylar::Env::getEnvr()->addHelp("s", "start exe");
    sylar::Env::getEnvr()->addHelp("t", "stop exe");
    if (!sylar::Env::getEnvr()->init(argc, argv)) {
        sylar::Env::getEnvr()->printHelp();
        return 0;
    }
    sylar::Env::getEnvr()->printHelp();
    SYLAR_LOG_DEBUG(g_logger) << "exe: " << sylar::Env::getEnvr()->getExe();
    SYLAR_LOG_DEBUG(g_logger) << "cwd: " << sylar::Env::getEnvr()->getCwd();
    SYLAR_LOG_DEBUG(g_logger) << "get PATH: " << sylar::Env::getEnvr()->getEnv("PATH", "XXX");
    SYLAR_LOG_DEBUG(g_logger) << "get test: " << sylar::Env::getEnvr()->getEnv("test", "XXX");
    SYLAR_LOG_DEBUG(g_logger) << "set test NXX: " << sylar::Env::getEnvr()->setEnv("test", "NXX");
    SYLAR_LOG_DEBUG(g_logger) << "get test: " << sylar::Env::getEnvr()->getEnv("test", "XXX");
    return 0;
}