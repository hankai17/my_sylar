#include "util.hh"
#include "log.hh"
#include "fiber.hh"

#include <execinfo.h>
#include <sys/time.h>

namespace sylar {
    //sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    pid_t GetThreadId() {
        return syscall(SYS_gettid);
    }

    uint64_t GetFiberId() {
        //return Fiber::GetThis()->getFiberId(); // It may bad_weak_ptr
        return Fiber::GetFiberId();
    }

    //size: n items
    void Backtrace(std::vector<std::string> &bt, int size, int skip) {
        void** array = (void**)malloc(sizeof(void*) * size);
        size_t s = ::backtrace(array, size);
        char** strings = backtrace_symbols(array, size);
        if (strings == NULL) {
            SYLAR_LOG_ERROR(SYLAR_LOG_NAME("system")) << "backtrace_symbols failed";
            free(array);
            return;
        }

        for (size_t i = skip; i < s; i++) {
            bt.push_back(strings[i]);
        }

        free(array);
        free(strings);
    }

    std::string BacktraceToString(int size, int skip, const std::string& prefix) { // Why skip two layers?
        std::vector<std::string> bt;
        Backtrace(bt, size, skip);
        std::stringstream ss;
        for (size_t i = 0; i < bt.size(); i++) {
            ss << prefix << bt[i] << std::endl;
        }
        return ss.str();
    }

    uint64_t GetCurrentMs() {
        struct timeval time;
        gettimeofday(&time, NULL);
        //return time.tv_sec * 1000 + time.tv_usec / 1000;
        return time.tv_sec * 1000ul + time.tv_usec / 1000;
    }

    uint64_t GetCurrentUs() {
        struct timeval time;
        gettimeofday(&time, NULL);
        return time.tv_sec * 1000 * 1000ul + time.tv_usec;
    }
}
