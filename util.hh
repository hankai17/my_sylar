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
#include <iomanip>
#include <memory>
#include <boost/lexical_cast.hpp>

namespace sylar {
    pid_t GetThreadId();
    uint64_t GetFiberId();
    void Backtrace(std::vector<std::string> &bt, int size, int skip = 1);
    std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = " ");
    uint64_t GetCurrentMs();
    uint64_t GetCurrentUs();
    std::string Time2Str(time_t ts = time(0), const std::string& format = "%Y-%m-%d %H:%M:%S");

    class FSUtil {
    public:
        typedef std::shared_ptr<FSUtil> ptr;
        static void ListAllFile(std::vector<std::string>& files, const std::string& path, const std::string& subfix);
        static bool Mkdir(const std::string& dirname);
        static bool Rm(const std::string& path);
        static bool Mv(const std::string& from, const std::string& to);
        static bool Symlink(const std::string& form, const std::string& to);
        static bool Unlink(const std::string& filename, bool exist = false);
        static std::string Dirname(const std::string& filename);
        static std::string Basename(const std::string& filename);
        static bool OpenForRead(std::ifstream& ifs, const std::string& filename, std::ios_base::openmode mode);
        static bool OpenForWrite(std::ofstream& ofs, const std::string& filename, std::ios_base::openmode mode);
        static bool Realpath(const std::string& path, std::string& rpath);
        static bool IsRunningPidfile(const std::string& pidfile);
    };

    template <typename Map, typename K, typename V>
    V GetParaValue(const Map& m, const K& k, const V& def = V()) {
        auto it = m.find(k);
        if (it == m.end()) {
            return def;
        }
        try {
            return boost::lexical_cast<V>(it->second);
        } catch (...) {
        }
        return def;
    }

    template <typename Map, typename K, typename V>
    bool GetParaValue(const Map& m, const K& k, V& v) {
        auto it = m.find(k);
        if (it == m.end()) {
            return false;
        }
        try {
            return boost::lexical_cast<V>(it->second);
        } catch (...) {
        }
        return false;
    }

    class Atomic {
    public:
        template<class T, class S>
        static T addFetch(volatile T &t, S v = 1) {
            return __sync_add_and_fetch(&t, (T) v);
        }

        template<class T, class S>
        static T subFetch(volatile T &t, S v = 1) {
            return __sync_sub_and_fetch(&t, (T) v);
        }

        template<class T, class S>
        static T orFetch(volatile T &t, S v) {
            return __sync_or_and_fetch(&t, (T) v);
        }

        template<class T, class S>
        static T andFetch(volatile T &t, S v) {
            return __sync_and_and_fetch(&t, (T) v);
        }

        template<class T, class S>
        static T xorFetch(volatile T &t, S v) {
            return __sync_xor_and_fetch(&t, (T) v);
        }

        template<class T, class S>
        static T nandFetch(volatile T &t, S v) {
            return __sync_nand_and_fetch(&t, (T) v);
        }

        template<class T, class S>
        static T fetchAdd(volatile T &t, S v = 1) {
            return __sync_fetch_and_add(&t, (T) v);
        }

        template<class T, class S>
        static T fetchSub(volatile T &t, S v = 1) {
            return __sync_fetch_and_sub(&t, (T) v);
        }

        template<class T, class S>
        static T fetchOr(volatile T &t, S v) {
            return __sync_fetch_and_or(&t, (T) v);
        }

        template<class T, class S>
        static T fetchAnd(volatile T &t, S v) {
            return __sync_fetch_and_and(&t, (T) v);
        }

        template<class T, class S>
        static T fetchXor(volatile T &t, S v) {
            return __sync_fetch_and_xor(&t, (T) v);
        }

        template<class T, class S>
        static T fetchNand(volatile T &t, S v) {
            return __sync_fetch_and_nand(&t, (T) v);
        }

        template<class T, class S>
        static T compareAndSwap(volatile T &t, S old_val, S new_val) {
            return __sync_val_compare_and_swap(&t, (T) old_val, (T) new_val);
        }

        template<class T, class S>
        static bool compareAndSwapBool(volatile T &t, S old_val, S new_val) {
            return __sync_bool_compare_and_swap(&t, (T) old_val, (T) new_val);
        }
    };

    class StringUtil {
    public:
        static std::string Format(const char* fmt, ...);
        static std::string Formatv(const char* fmt, va_list ap);
    };

    template<typename T>
    void nop(T*) {}
}

#endif