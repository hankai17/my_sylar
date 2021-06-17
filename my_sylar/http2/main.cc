#include <iostream>
#include <string>
#include <stdint.h>
#include <stdio.h>

#include "huffman_table.h"
#include "huffman.h"

void hf_print_hex(unsigned char *buff, int size) {
	static char hex[] = {'0','1','2','3','4','5','6','7',
								'8','9','a','b','c','d','e','f'};
	int i = 0;
	for (i = 0;i < size; i++) {
		unsigned char ch = buff[i];
		printf("(%u)%c", ch,hex[(ch>>4)]);
		printf("%c ", hex[(ch&0x0f)]);
	}
	printf("\n");
}

int hf_string_encode_len(unsigned char *enc, int enc_sz) {
    int i       = 0;
    int len     = 0;
    for (i = 0; i < enc_sz; i++) {
        len += sylar::http2::huffman_code_len[(int)enc[i]];
    }
    return (len+7)/8;
}

void testHuffman() {
    //std::string str = "hello huffman,你好,世界";
    std::string str = "\"5cb816f5-19d8\""; //"hello huffman";
    std::string out;
    sylar::http2::Huffman::EncodeString(str, out, 0);
    std::cout << "str.size=" << str.size()
              << " out.size=" << out.size()
              << std::endl;
    hf_print_hex((unsigned char*)out.c_str(), out.length());

    std::string str2;
    sylar::http2::Huffman::DecodeString(out, str2);
    std::cout << str2 << std::endl;
    std::cout << "str.size=" << str.size()
              << " out.size=" << out.size() << std::endl;

    std::cout << "hf_string_encode_len: " << hf_string_encode_len((uint8_t*)str.c_str(), str.size()) << std::endl;
}

int main()
{
    testHuffman();
    return 0;
}
