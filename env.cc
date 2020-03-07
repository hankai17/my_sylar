#include "env.hh"
#include "log.hh"
#include <string.h>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <stdlib.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    Env* Env::env = new Env;
    Env::Garbo Env::garbo;

    bool Env::init(int argc, char** argv) {
        char link[1024] = {0};
        char path[1024] = {0};
        sprintf(link, "/proc/%d/exe", getpid()); // exe是个软链
        readlink(link, path, sizeof(path));
        m_exe = path; // /root/CLionProjects/my_sylar/cmake-build-debug/env_test"
        /*
                readlink /proc/3158/exe
                /root/file/clion-2018.3.4/jre64/bin/java
        */
        auto pos = m_exe.find_last_of("/");
        m_cwd = m_exe.substr(0, pos) + "/"; // /root/CLionProjects/my_sylar/cmake-build-debug/"
        m_program = argv[0];
        const char* now_key = nullptr;
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] == '-') {
                if (strlen(argv[i]) > 1) {
                    if (now_key) {
                        add(now_key, "");
                    }
                    now_key = argv[i] + 1;
                } else {
                    SYLAR_LOG_ERROR(g_logger) << "invalid arg idx: " << i
                    << " value: " << argv[i];
                    return false;
                }
            } else {
                if (now_key) {
                    add(now_key, argv[i]);
                    now_key = nullptr;
                } else {
                    SYLAR_LOG_ERROR(g_logger) << "invalid arg idx: " << i
                                              << " value: " << argv[i];
                    return false;
                }
            }
        }
        if (now_key) {
            add(now_key, "");
            now_key = nullptr;
        }
        return true;
    }

    void Env::add(const std::string& key, const std::string& value) {
        RWMutexType::WriteLock lock(m_mutex);
        m_args[key] = value;
    }

    bool Env::has(const std::string& key) {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_args.find(key);
        return it != m_args.end();
    }

    void Env::del(const std::string& key) {
        RWMutexType::WriteLock lock(m_mutex);
        m_args.erase(key);
    }

    std::string Env::get(const std::string& key, const std::string& default_value) {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_args.find(key);
        return it == m_args.end() ? default_value : it->second;
    }

    void Env::addHelp(const std::string& key, const std::string& desc) {
        removeHelp(key);
        RWMutexType::WriteLock lock(m_mutex);
        m_helps.push_back(std::make_pair(key, desc));
    }

    void Env::removeHelp(const std::string& key) {
        RWMutexType::WriteLock lock(m_mutex);
        for (auto it = m_helps.begin(); it != m_helps.end(); ) {
            if (it->first == key) {
                it = m_helps.erase(it);
            } else {
                ++it;
            }
        }
    }

    void Env::printHelp() {
        RWMutexType::ReadLock lock(m_mutex);
        std::cout << "Usage: " << m_program << " [options]" << std::endl;
        for (const auto& i : m_helps) {
            std::cout << std::setw(5) << "-" << i.first << " : "
            << i.second << std::endl;
        }
    }

    bool Env::setEnv(const std::string& key, const std::string& value) {
        return !setenv(key.c_str(), value.c_str(), 1);

    }

    std::string Env::getEnv(const std::string& key, const std::string& default_value) {
        const char* v = getenv(key.c_str());
        if (v == nullptr) {
            return default_value;
        }
        return v;
    }

    std::string Env::getAbsolutPath(const std::string& path) const {
        if (path.empty()) {
            return "/";
        }
        if (path[0] == '/') {
            return path;
        }
        return m_cwd + path;
    }
}