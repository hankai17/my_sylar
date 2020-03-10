#ifndef __APPLICATION_HH__
#define __APPLICATION_HH__

#include <memory>
#include <vector>
#include "iomanager.hh"
#include "http/http_server.hh"
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
        std::string getAbsolutPath(const std::string& path) const;
        std::string getConfPath();

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

    class Application {
    public:
        typedef std::shared_ptr<Application> ptr;
        static Application* GetInstance () { return s_instance; }
        Application();
        bool init(int argc, char** argv);
        bool run();

    private:
        int main(int argc, char** argv);
        int run_fiber();
    private:
        int                 m_argc = 0;
        char**              m_argv = nullptr;
        IOManager::ptr      m_mainIOmanager;
        static Application* s_instance;
        std::vector<sylar::http::HttpServer::ptr>  m_httpservers;
    };
}

#endif