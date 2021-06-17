#ifndef __HUFFMAN_HH__
#define __HUFFMAN_HH__

#include <string>

namespace sylar {
namespace http2 {

class Huffman {
public:
    static int EncodeString(const char* in, int in_len, std::string& out, int prefix);
    static int EncodeString(const std::string& in, std::string& out, int prefix);
    static int DecodeString(const char* in, int in_len, std::string& out);
    static int DecodeString(const std::string& in, std::string& out);
    static int EncodeLen(const std::string& in);
    static int EncodeLen(const char* in, int in_len);

    static bool ShouldEncode(const std::string& in);
    static bool ShouldEncode(const char* in, int in_len);
};

//void testHuffman();

}
}

#endif
