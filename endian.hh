#ifndef __ENDIAN_HH__
#define __ENDIAN_HH__

#include <byteswap.h>
#include <stdint.h>

//高放高低放低小端
//网络字节顺序采用大端排序方式
#define SYLAR_LITTLE_ENDIAN 1
#define SYLAR_BIG_ENDIAN 2

namespace sylar {

    template <typename T>
    typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
    byteswap(T value) {
        return (T)bswap_64((uint64_t)value);
    }

    template <typename T>
    typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
    byteswap(T value) {
        return (T)bswap_32((uint32_t)value);
    }

    template <typename T>
    typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
    byteswap(T value) {
        return (T)bswap_16((uint16_t)value);
    }

#if BYTE_ORDER == BIG_ENDIAN
#define SYLAR_BYTE_ORDER SYLAR_BIG_ENDIAN
#else
#define SYLAR_BYTE_ORDER SYLAR_LITTLE_ENDIAN
#endif

#if SYLAR_BYTE_ORDER == SYLAR_BIG_ENDIAN
    //TODO
#else
    template <typename T>
    T byteswapOnLittleEndian(T t) {
        return byteswap(t);
    }

    template <typename T>
    T byteswapOnBigEndian(T t) {
        return t;
    }
#endif

}
#endif
