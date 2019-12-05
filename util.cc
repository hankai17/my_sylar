#include "util.hh"

namespace sylar {
    pid_t GetThreadId() {
        return syscall(SYS_gettid);
    }

    uint32_t GetFiberId() {
        return 9;
    }
}
