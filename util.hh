#ifndef __UTIL_HH__
#define __UTIL_HH__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>

namespace sylar {
    pid_t GetThreadId();
    uint64_t GetFiberId();
    void Backtrace(std::vector<std::string> &bt, int size, int skip = 1);
    std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = " ");
    uint64_t GetCurrentMs();
    uint64_t GetCurrentUs();
    std::string Time2Str(time_t ts = time(0), const std::string& format = "%Y-%m-%d %H:%M:%S");
}

#endif