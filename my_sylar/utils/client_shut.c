#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERV_IP "127.0.0.1"
#define SERV_PORT 9527

int main(void)
{
    int sfd, len;
    struct sockaddr_in serv_addr;
    int sum_read = 0;
    char buf[BUFSIZ] = "GET /2.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"; 

    sfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serv_addr, sizeof(serv_addr));                       //清零
    serv_addr.sin_family = AF_INET;                             //IPv4协议族
    inet_pton(AF_INET, SERV_IP, &serv_addr.sin_addr.s_addr);    //指定IP 字符串类型转换为网络字节序 参3:传出参数
    serv_addr.sin_port = htons(SERV_PORT);                      //指定端口 本地转网络字节序

    if (0) {
        struct linger l;
        l.l_onoff = 1;
        l.l_linger = 0;
        setsockopt(sfd, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l));
    }

    connect(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (1) {
        int ret = write(sfd, buf, strlen(buf));
        printf("send ret: %d\n", ret);
		memset(buf, 0x0, BUFSIZ);

        if((len = read(sfd, buf, sizeof(buf))) > 0) {
          sum_read+=len;
          printf("total read: %d\n", sum_read);
        }

        //shutdown(sfd, SHUT_RD);
        //shutdown(sfd, SHUT_RDWR);

        //shutdown(sfd, SHUT_WR);
        close(sfd);
    }
    printf("fd: %d, now we block...\n", sfd);

    while(1);
    while(1) {
        len = read(sfd, buf, sizeof(buf));
        if (len > 0) {
          len+=len;
          printf("total read: %d\n", len);
        }
    }



    /*关闭链接*/
    printf("had closed, still read\n");
    //len = read(sfd, buf, sizeof(buf));
    //write(STDOUT_FILENO, buf, len);
    sleep(100);

    return 0;
}
