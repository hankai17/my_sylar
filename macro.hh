#ifndef __MACRO_HH__
#define __MACRO_HH__

#include "log.hh"

#define SYLAR_ASSERT(x) \
    if (!(x)) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x \
        << "\nbacktrace:\n"
<< sylar::Back

}

#endif