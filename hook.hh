#ifndef __HOOK_HH__
#define __HOOK_HH__

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

namespace sylar {
    bool is_hook_enable();
    void set_hook_enable(bool flag);
}

extern "C" {
    // sleep
    typedef unsigned int (*sleep_fun)(unsigned int seconds);
    extern sleep_fun sleep_f;

    typedef int (*usleep_fun)(useconds_t usec);
    extern usleep_fun usleep_f;
}

#endif