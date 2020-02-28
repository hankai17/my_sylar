#ifndef __ENV_HH__
#define __ENV_HH__

#include <memory>
#include <string>
#include <map>
#include <vector>
#include "thread.hh"

namespace sylar {
    class Env {
    public:
        typedef std::shared_ptr<Env> ptr;
        typedef RWMutex RWMutexType;
        bool init(int argc, char** argv);
        void add(const std::string& key, const std::string& value);
        bool has(const std::string& key);
        void del(const std::string& key);
        std::string get(const std::string& key, const std::string& default_value = "");

        void addHelp(const std::string& key, const std::string& desc);
        void removeHelp(const std::string& key);
        void printHelp();

        const std::string& getExe() const { return m_exe; }
        const std::string& getCwd() const { return m_cwd; }

        bool setEnv(const std::string& key, const std::string& value);
        std::string getEnv(const std::string& key, const std::string& default_value = "");
        static Env* getEnvr() { return env; };

    private:
        class Garbo {
        public:
            ~Garbo() {
                if (env != nullptr) {
                    delete env;
                }
            }
        };
        static Env*         env;
        static Garbo        garbo;

        std::string         m_exe;
        std::string         m_cwd;
        RWMutexType         m_mutex;
        std::string         m_program;
        std::map<std::string, std::string> m_args;
        std::vector<std::pair<std::string, std::string> > m_helps;
    };
}

#endif
