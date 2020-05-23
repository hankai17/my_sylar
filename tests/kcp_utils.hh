#ifndef __KCP_UTILS_HH__
#define __KCP_UTILS_HH__

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string>

#define SYLAR_KCP_CONN_PACK "sylar kcp connect package get conv"
#define SYLAR_KCP_RESP_CONV_PACK "sylar kcp response conv package: "
#define SYLAR_KCP_DIS_CONN_PACK "sylar kcp disconnect package"

std::string make_connect_pack();
bool is_connect_pack(const char* data, size_t len);

std::string make_response_conv_pack(uint32_t conv);
bool is_response_conv_pack(const char* data, size_t len);
uint32_t parse_conv_from_response_conv_pack(const char* data, size_t len);

std::string make_disconnect_pack(uint32_t conv);
bool is_disconnect_pack(const char* data, size_t len);
uint32_t parse_conv_from_disconnect_pack(const char* data, size_t len);


std::string make_connect_pack() {
    return std::string(SYLAR_KCP_CONN_PACK, sizeof(SYLAR_KCP_CONN_PACK));
}

bool is_connect_pack(const char* data, size_t len) {
    return len == strlen(SYLAR_KCP_CONN_PACK) &&
        memcmp(data, SYLAR_KCP_CONN_PACK, strlen(SYLAR_KCP_CONN_PACK)) == 0;
}

std::string make_response_conv_pack(uint32_t conv) {
    char response_conv[256] = "";
    size_t n = snprintf(response_conv, sizeof(response_conv), "%s%u", SYLAR_KCP_RESP_CONV_PACK, conv);
    return std::string(response_conv, n);
}

bool is_response_conv_pack(const char* data, size_t len) {
    return len > strlen(SYLAR_KCP_RESP_CONV_PACK) &&
        memcmp(data, SYLAR_KCP_RESP_CONV_PACK, strlen(SYLAR_KCP_RESP_CONV_PACK)) == 0;
}

uint32_t parse_conv_from_response_conv_pack(const char* data, size_t len) {
    uint32_t conv = atol(data + sizeof(SYLAR_KCP_RESP_CONV_PACK));
    return conv;
}

std::string make_disconnect_pack(uint32_t conv) {
    char response_conv[256] = "";
    size_t n = snprintf(response_conv, sizeof(response_conv), "%s %u", SYLAR_KCP_DIS_CONN_PACK, conv);
    return std::string(response_conv, n);
}

bool is_disconnect_pack(const char* data, size_t len) {
    return len > strlen(SYLAR_KCP_DIS_CONN_PACK) &&
        memcmp(data, SYLAR_KCP_DIS_CONN_PACK, strlen(SYLAR_KCP_DIS_CONN_PACK)) == 0;
}

uint32_t parse_conv_from_disconnect_pack(const char* data, size_t len) {
    uint32_t conv = atol(data + sizeof(SYLAR_KCP_DIS_CONN_PACK));
    return conv;
}

#endif