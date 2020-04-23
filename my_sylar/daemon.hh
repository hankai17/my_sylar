#ifndef __DAEMON_HH__
#define __DAEMON_HH__

#include <memory>
#include <unistd.h>
#include <functional>
#include <string>

namespace sylar {
    struct ProcessInfo {
    public:
        pid_t       parent_id = 0;
        pid_t       main_id = 0;
        uint64_t    parent_start_time = 0;
        uint64_t    main_start_time = 0;
        uint32_t    restart_count = 0;
        std::string toString() const;
        static ProcessInfo* getProcessInfo() { return proInfo; };

    private:
        class Garbo {
            //private:
        public:
            ~Garbo() {
                if (proInfo != nullptr) {
                    delete proInfo;
                }
            }
        };
        ProcessInfo() {};
        static Garbo garbo;
        static ProcessInfo* proInfo;
    };

    int start_daemon(int argc, char** argv,
            std::function<int(int argc, char** argv)> main_cb, bool is_daemon);

}

#endif
