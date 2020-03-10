#include "application.hh"
#include "log.hh"
#include "config.hh"
#include "util.hh"
#include "daemon.hh"
#include "worker.hh"
#include "address.hh"
#include "iomanager.hh"
#include "http/http_server.hh"

#include <vector>

namespace sylar {
    static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

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

    std::string Env::getConfPath() {
        return getAbsolutPath(get("c", "conf"));
    }

    static sylar::ConfigVar<std::string>::ptr g_server_work_path =
            sylar::Config::Lookup("server.work_path", std::string("/usr/local/my_sylar"), "server work path");
    static sylar::ConfigVar<std::string>::ptr g_server_pid_file =
            sylar::Config::Lookup("server.pid_file", std::string("sylar.pid"), "server pid file");

    struct HttpServerConf {
        int keepalive = 0;
        int timeout = 1000 * 2 * 60;
        int ssl = 0;
        std::string name;
        std::string cert_file;
        std::string key_file;
        std::string accept_worker;
        std::string io_worker;
        std::vector<std::string> addresses;

        bool isValid() const {
            return !addresses.empty();
        }

        bool operator== (const HttpServerConf& conf) const {
            return keepalive == conf.keepalive &&
            timeout == conf.timeout &&
            ssl == conf.ssl &&
            name == conf.name &&
            cert_file == conf.cert_file &&
            key_file == conf.key_file &&
            accept_worker == conf.accept_worker &&
            io_worker == conf.io_worker &&
            addresses == conf.addresses;
        }
    };
    static sylar::ConfigVar<std::vector<HttpServerConf> >::ptr g_http_server =
            sylar::Config::Lookup("http_servers", std::vector<HttpServerConf> {}, "http server config");

    struct HttpServerConfigIniter {
        HttpServerConfigIniter() {
            g_http_server->addListener("http_servers", [](const std::vector<HttpServerConf>& old_val,
                    const std::vector<HttpServerConf>& new_val) {
                const_cast<
                    std::vector<HttpServerConf>&
                > (old_val) = new_val;
            });
        }
    };
    static HttpServerConfigIniter __httpserver_config_init;
    /*
    http_servers:
    - address: ["0.0.0.0:8090", "127.0.0.1:8091", "/tmp/test.sock"]
    keepalive: 1
    timeout: 1000
    name: sylar/1.1
    accept_worker: accept
    process_worker:  io

    - address: ["0.0.0.0:8072", "localhost:8071"]
    keepalive: 1
    timeout: 1000
    name: sylar/2.1
    accept_worker: accept
    process_worker:  io
   */
    template<>
    //class Lexicalcast<std::string, std::vector<HttpServerConf> >
    class Lexicalcast<std::string, HttpServerConf> {
    public:
        HttpServerConf operator() (std::string v) {
            YAML::Node node = YAML::Load(v);
            HttpServerConf conf;
            conf.keepalive = node["keepalive"].as<int>(conf.keepalive);
            conf.timeout = node["timeout"].as<int>(conf.timeout);
            conf.ssl = node["ssl"].as<int>(conf.ssl);
            conf.name = node["name"].as<std::string>(conf.name);
            conf.cert_file = node["cert_file"].as<std::string>(conf.cert_file);
            conf.key_file = node["key_file"].as<std::string>(conf.key_file);
            conf.accept_worker = node["accept_worker"].as<std::string>(conf.accept_worker);
            conf.io_worker = node["io_worker"].as<std::string>(conf.io_worker);
            if (node["addresses"].IsDefined()) {
                for (const auto& i : node["addresses"]) {
                    conf.addresses.push_back(i.as<std::string>());
                }
            }
            return conf;
        }
    };

    template<>
    class Lexicalcast<HttpServerConf, std::string> {
    public:
        std::string operator() (const HttpServerConf& conf) {
            YAML::Node node;
            node["keepalive"] = conf.keepalive;
            node["timeout"] = conf.timeout;
            node["ssl"] = conf.ssl;
            node["name"] = conf.name;
            node["cert_file"] = conf.cert_file;
            node["key_file"] = conf.key_file;
            node["accept_worker"] = conf.accept_worker;
            node["io_worder"] = conf.io_worker;
            for (const auto& i : conf.addresses) {
                node["addresses"].push_back(i);
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    Application* Application::s_instance = new Application;

    Application::Application() {
        //s_instance = this;
    }

    bool Application::init(int argc, char** argv) {
        m_argc = argc;
        m_argv = argv;
        sylar::Env::getEnvr()->addHelp("s", "start with terminal");
        sylar::Env::getEnvr()->addHelp("d", "run as daemon");
        sylar::Env::getEnvr()->addHelp("c", "config path default: ./conf");
        sylar::Env::getEnvr()->addHelp("h", "print help");

        if (!sylar::Env::getEnvr()->init(argc, argv)) {
            sylar::Env::getEnvr()->printHelp();
            return false;
        }

        if (sylar::Env::getEnvr()->has("p")) {
            sylar::Env::getEnvr()->printHelp();
            return false;
        }

        int run_type = 0;
        if (sylar::Env::getEnvr()->has("s")) {
            run_type = 1;
        }
        if (sylar::Env::getEnvr()->has("d")) {
            run_type = 2;
        }

        if (run_type == 0) {
            sylar::Env::getEnvr()->printHelp();
            //return false;
        }

        std::string conf_path = sylar::Env::getEnvr()->getAbsolutPath(
                sylar::Env::getEnvr()->get("c", "conf")
                );
        SYLAR_LOG_DEBUG(g_logger) << "load config path: " << conf_path;
        sylar::Config::loadFromConfDir(conf_path);
        // for load check
        {
        }
        std::string pidfile = g_server_work_path->getValue() + "/" +
                g_server_pid_file->getValue();
        if (sylar::FSUtil::IsRunningPidfile(pidfile)) {
            SYLAR_LOG_ERROR(g_logger) << "server is running";
            return false;
        }

        if (!sylar::FSUtil::Mkdir(g_server_work_path->getValue())) {
            SYLAR_LOG_ERROR(g_logger) << "cant create dir: " << g_server_work_path->getValue()
            << " errno: " << errno << " strerrno: " << strerror(errno);
            return false;
        }
        return true;
    }

    bool Application::run() {
        bool is_daemon = sylar::Env::getEnvr()->has("d");
        return start_daemon(m_argc, m_argv, std::bind(&Application::main, this,
                std::placeholders::_1,
                std::placeholders::_2), is_daemon);
    }

    int Application::main(int argc, char** argv) {
        SYLAR_LOG_DEBUG(g_logger) << "main";
        {
            std::string pid_file = g_server_work_path->getValue() + "/" +
                    g_server_pid_file->getValue();
            std::ofstream ofs(pid_file);
            if (!ofs) {
                SYLAR_LOG_ERROR(g_logger) << "open pidfile: " << pid_file
                << "failed";
                return false;
            }
            ofs << getpid();
        }
        m_mainIOmanager.reset(new sylar::IOManager(1, false, "main")); // TODO sylar::IOManager(1, true, "main"));
        m_mainIOmanager->schedule(std::bind(&Application::run_fiber, this));
        m_mainIOmanager->addTimer(1000, [](){}, true);
        m_mainIOmanager->stop();
        return 0;
    }

    int Application::run_fiber() {
        sylar::WorkerManager::GetWorkMgr()->init();
        auto http_confs = g_http_server->getValue();
        for (const auto& i : http_confs) {
            SYLAR_LOG_ERROR(g_logger) << Lexicalcast<HttpServerConf, std::string>()(i);
            std::vector<Address::ptr> address;
            for (const auto& j : i.addresses) {
                size_t pos = j.find(":");
                if (pos == std::string::npos) {
                    //address.
                    SYLAR_LOG_ERROR(g_logger) << "address err no port: " << j;
                    continue;
                }
                int32_t port = atoi(j.substr(pos + 1).c_str());
                auto addr = sylar::IPAddress::Create(j.substr(0, pos).c_str(), port);
                if (addr) {
                    address.push_back(addr);
                    continue;
                }
                // not support localhost Only support ip:port string
            }
            IOManager* accept_worker = IOManager::GetThis();
            IOManager* io_worker = IOManager::GetThis();
            if (!i.accept_worker.empty()) {
                accept_worker = WorkerManager::GetWorkMgr()->getAsIOManager(i.accept_worker).get();
                if (!accept_worker) {
                    SYLAR_LOG_ERROR(g_logger) << "accept_worker: " << i.accept_worker << " not exists";
                    _exit(0);
                }
            }
            if (!i.io_worker.empty()) {
                io_worker = WorkerManager::GetWorkMgr()->getAsIOManager(i.io_worker).get();
                if (!io_worker) {
                    SYLAR_LOG_ERROR(g_logger) << "io_worker: " << i.io_worker << " not exists";
                    _exit(0);
                }
            }

            sylar::http::HttpServer::ptr server(new sylar::http::HttpServer(i.keepalive,
                    io_worker, accept_worker));
            std::vector<Address::ptr> fails;
            if (!server->bind(address, fails, i.ssl)) {
                for (const auto& j : fails) {
                    SYLAR_LOG_ERROR(g_logger) << "bind address failed: " << j->toString();
                }
                _exit(0);
            }
            if (i.ssl) {
                if (!server->loadCertificates(i.cert_file, i.key_file)) { // Must absolute path
                    SYLAR_LOG_ERROR(g_logger) << "loadCertificates failed, cert_file: "
                    << i.cert_file << " key_file: " << i.key_file;
                    _exit(0);
                }
            }
            if (!i.name.empty()) {
                server->setName(i.name);
            }
            server->start();
            m_httpservers.push_back(server);
        }
        return 0;
    }

}