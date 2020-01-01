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
}

#endif