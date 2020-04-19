#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERV_IP "127.0.0.1"
#define SERV_PORT 90

int main(void)
{
    int sfd, len;
    struct sockaddr_in serv_addr;
    //char buf[BUFSIZ] = "GET /test HTTP/1.1\r\nAccept: */*\r\nHost: 127.0.0.1:9527\r\n\r\n"; 
    char buf[BUFSIZ] = "GET /2.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"; 

    /*创建一个socket 指定IPv4 TCP*/
    sfd = socket(AF_INET, SOCK_STREAM, 0);

    /*初始化一个地址结构:*/
    bzero(&serv_addr, sizeof(serv_addr));                       //清零
    serv_addr.sin_family = AF_INET;                             //IPv4协议族
    inet_pton(AF_INET, SERV_IP, &serv_addr.sin_addr.s_addr);    //指定IP 字符串类型转换为网络字节序 参3:传出参数
    serv_addr.sin_port = htons(SERV_PORT);                      //指定端口 本地转网络字节序

    /*根据地址结构链接指定服务器进程*/
    connect(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    int count_flag = 10;
    int i = 0;

    if (1) {
        write(sfd, buf, strlen(buf));
		memset(buf, 0x0, BUFSIZ);
        //len = read(sfd, buf, sizeof(buf));
        //write(STDOUT_FILENO, buf, len);

        while((len = read(sfd, buf, sizeof(buf))) > 0) {
          i++; len+=len;
          if (i == count_flag) {
            shutdown(sfd, SHUT_WR);
            //shutdown(sfd, SHUT_RD);
            //shutdown(sfd, SHUT_RDWR);
            //close(sfd);
          }
          printf("total read: %d\n", len);
        }
    }
    printf("fd: %d, now we block...\n", sfd);
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

/*
shutdown 跟 close不能相互代替: shutdown后 fd仍被占(虽然 重新open一个fd 又是这个fd 但这肯定说明有泄漏)
fd的close一定是放到最后 肯定在某个抽象对象的析构中关闭 eg(stream) 所以不要尝试着手动的调close函数  可以手动调shutdown
close早了不好 eg我遇到的 被close的fd 在es中仍然有事件 即还在epoll树上挂着   而突然来的链接恰好占用了这个fd...   被我es中 同一个fd不能再次添加同一个事件捕获
*/
