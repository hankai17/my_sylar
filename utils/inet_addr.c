#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <iostream>
#include <string.h>
using namespace std;

int main() {
    char ip1[] = "192.168.0.74";
    char ip2[] = "211.100.21.179";
    long l1, l2;
    in_addr addr1, addr2;

    l1 = inet_addr(ip1);   //将字符串形式的IP地址 -> 网络字节顺序  的整型值
    l2 = inet_addr(ip2);
    printf("IP1: %s\n IP2: %s\n", ip1, ip2);
    printf("Addr1: %ld\n Addr2: %ld\n", l1, l2);

    memcpy(&addr1, &l1, 4); //复制4个字节大小
    memcpy(&addr2, &l2, 4);
    printf("%s <--> %s\n", inet_ntoa(addr1),
           inet_ntoa(addr2)); //注意：printf函数自右向左求值、覆盖

    //printf("%s\n", inet_ntoa(addr1)); //网络字节顺序的整型值 ->字符串形式的IP地址
    //printf("%s\n", inet_ntoa(addr2));
    return 0;
}
