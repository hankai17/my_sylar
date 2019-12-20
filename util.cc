#include "util.hh"
#include "log.hh"

namespace sylar {
    sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    pid_t GetThreadId() {
        return syscall(SYS_gettid);
    }

    uint32_t GetFiberId() {
        return 9;
    }

    void Backtrace(std::vector<std::string> &bt, int size, int skip = 1) {

    }

    std::string BacktraceToString(int size, int skip = 2, const std::string &prefix = " ") {

    }
}
