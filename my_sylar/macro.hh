#ifndef __MACRO_HH__
#define __MACRO_HH__

#include "log.hh"
#include "util.hh"
#include <assert.h>

#if defined __GNUC__ || defined __llvm__
#   define SYLAR_LICKLY(x)       __builtin_expect(!!(x), 1)
#   define SYLAR_LIKELY(x)       __builtin_expect(!!(x), 1)
#   define SYLAR_UNLICKLY(x)     __builtin_expect(!!(x), 0)
#   define SYLAR_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define SYLAR_LICKLY(x)      (x)
#   define SYLAR_LIKELY(x)      (x)
#   define SYLAR_UNLICKLY(x)      (x)
#   define SYLAR_UNLIKELY(x)      (x)
#endif


#define SYLAR_ASSERT(x) \
    if (!(x)) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x \
        << "\nbacktrace:\n" \
        << sylar::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif