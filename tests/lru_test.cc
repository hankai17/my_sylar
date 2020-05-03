#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/utils/lru.hh"
#include "my_sylar/utils/rate_limiter.hh"
#include <iostream>

void test_lru() {
    sylar::LruCache<int, int> cache(30, 10);
    for (int i = 0; i < 105; ++i) {
        cache.set(i, i * 100);
    }

    for (int i = 0; i < 105; ++i) {
        int v;
        if (cache.get(i, v)) {
            std::cout << "get: " << i << " - " << v << std::endl;
        }
    }
    std::cout << cache.toStatusString() << std::endl;
}

void test_hash_lru() {
    sylar::HashLruCache<int, int> cache(2, 30, 10);
    for (int i = 0; i < 105; ++i) {
        cache.set(i, i * 100);
    }
    for (int i = 0; i < 105; ++i) {
        int v;
        if (cache.get(i, v)) {
            std::cout << "get: " << i << " - " << v << std::endl;
        }
    }
    std::cout << cache.toStatusString() << std::endl;
}

void test_timed_cache() {
    sylar::TimedCache<int, int> cache(30, 10);
    for (int i = 0; i < 105; ++i) {
        cache.set(i, i * 100, 1000);
    }

    for (int i = 0; i < 105; ++i) {
        int v;
        if (cache.get(i, v)) {
            std::cout << "get: " << i << " - "
                      << v
                      << " - " << cache.get(i)
                      << std::endl;
        }
    }

    cache.set(1000, 11, 1000 * 10);
    //std::cout << "expired: " << cache.expired(100, 1000 * 10) << std::endl;
    std::cout << cache.toStatusString() << std::endl;
    sleep(2);
    std::cout << "check_timeout: " << cache.checkTimeout() << std::endl;
    std::cout << cache.toStatusString() << std::endl;
}

void test_hash_timed_cache() {
    sylar::HashTimedCache<int, int> cache(2, 30, 10);
    for (int i = 0; i < 105; ++i) {
        cache.set(i, i * 100, 1000 * i);
    }

    for (int i = 0; i < 105; ++i) {
        int v;
        if (cache.get(i, v)) {
            std::cout << "get: " << i << " - " << v << std::endl;
        }
    }
    cache.expired(100, 1000 * 10);
    cache.set(1000, 11, 1000 * 10);
    std::cout << "expired: " << cache.expired(100, 1000 * 10) << std::endl;
    std::cout << cache.toStatusString() << std::endl;
    sleep(2);
    std::cout << "check_timeout: " << cache.checkTimeout() << std::endl;
    std::cout << cache.toStatusString() << std::endl;
}

int main(int argc, char** argv) {
    if (0) {
        test_lru();
        test_hash_lru();
    } else {
        sylar::IOManager iom(1, false, "rate");
        sylar::RateLimiter limiter(iom, 3, 100000ull); // 100ms
        int ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;

        ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;

        ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;

        ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;

        ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;

        usleep(200000);

        ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;

        ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;

        ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;

        ret = limiter.allowed(1);
        std::cout << "ret: " << ret << std::endl;
    }

    return 0;
}