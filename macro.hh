#ifndef __MACRO_HH__
#define __MACRO_HH__

#include "log.hh"
#include "util.hh"
#include <assert.h>

#define SYLAR_ASSERT(x) \
    if (!(x)) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x \
        << "\nbacktrace:\n" \
        << sylar::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif