#include "daemon.hh"
#include <sstream>
#include "log.hh"
#include "config.hh"
#include "util.hh"

#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static sylar::ConfigVar<uint32_t >::ptr g_daemon_restart_interval =
            sylar::Config::Lookup("daemon.restart_interval", (uint32_t)5, "daemon restart interval");

    ProcessInfo* ProcessInfo::proInfo = new ProcessInfo;
    ProcessInfo::Garbo ProcessInfo::garbo;

    std::string ProcessInfo::toString() const {
        std::stringstream ss;
        ss << "parent_id: " << parent_id
        << " main_id: " << main_id
        << " parent_start_time: " << sylar::Time2Str(parent_start_time)
        << " main_start_time: " << sylar::Time2Str(main_start_time)
        << " restart_count: " << restart_count;
        return ss.str();
    }

    static int real_start(int argc, char** argv,
            std::function<int(int argc, char** argv)> main_cb) {
        return main_cb(argc, argv);
    }

    static int real_daemon(int argc, char** argv,
            std::function<int(int argc, char** argv)> main_cb) {
        daemon(1, 0);
        ProcessInfo::getProcessInfo()->parent_id = getpid();
        ProcessInfo::getProcessInfo()->parent_start_time = time(0);
        while (true) {
            pid_t pid = fork();
            if (pid == 0) { // child
                ProcessInfo::getProcessInfo()->main_id = getpid();
                ProcessInfo::getProcessInfo()->main_start_time = time(0);
                SYLAR_LOG_DEBUG(g_logger) << "Process start pid: " << getpid();
                return real_start(argc, argv, main_cb);
            } else if (pid < 0) {
                SYLAR_LOG_DEBUG(g_logger) << "fork failed, errno: " <<  errno
                << " strerror: " << strerror(errno);
                return -1;
            } else {
                int status = 0;
                waitpid(pid, &status, 0);
                if (status) {
                    SYLAR_LOG_DEBUG(g_logger) << "child crashed, pid: " << pid
                    << " status: " << status;
                } else {
                    SYLAR_LOG_DEBUG(g_logger) << "child finished";
                    break;
                }
                ProcessInfo::getProcessInfo()->restart_count += 1;
                sleep(g_daemon_restart_interval->getValue());
            }
        }
        return 0;
    }

    int start_daemon(int argc, char** argv,
                     std::function<int(int argc, char** argv)> main_cb, bool is_daemon) {
        if (!is_daemon) {
            return real_start(argc, argv, main_cb);
        }
        return real_daemon(argc, argv, main_cb);
    }

}