#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
 
#define SERVPORT 9527
#define MAXBUF 4096

int setnonblocking(int fd) {
    int opts;
    if( (opts = fcntl(fd, F_GETFL, 0)) == -1) {
        perror("fcntl");
        return -1;
    }
 
    opts = opts | O_NONBLOCK;
    if( (opts = fcntl(fd, F_SETFL, opts)) == -1) {
        perror("fcntl");
        return -1;
    }
 
    return 0;
}
 
int main(void)
{
    char buf[MAXBUF];
    struct sockaddr_in servaddr;
    int fd;
    int n, len;
 
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port = htons(SERVPORT);
 
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    //setnonblocking(fd);
    if (1) {
        struct linger l;
        l.l_onoff = 1;
        l.l_linger = 0;
        setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l));
    }

    int nSendBuf = 3072;
    socklen_t opt_len = sizeof(int);
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &nSendBuf, sizeof(int));
    if (ret == -1) {
        perror("setsockopt");
    }
    if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("connect");
        exit(1);
    }
 
    printf("Connection ok, socket will be shutdown after sleep 5s\n");
    sleep(5);

    printf("shutdown read\n");
    //shutdown(fd, SHUT_RD);
    shutdown(fd, SHUT_WR);

    while(1);
    sleep(5);
 
    send(fd, "I close RD, But i can write.", sizeof("I close RD, But i can write.") - 1, 0);
    printf("close\n");
    close(fd);
    return 0;
}
 
/*
    EPOLLIN = 0x001,
#define EPOLLIN EPOLLIN
    EPOLLPRI = 0x002,
#define EPOLLPRI EPOLLPRI
    EPOLLOUT = 0x004,
#define EPOLLOUT EPOLLOUT
    EPOLLRDNORM = 0x040,
#define EPOLLRDNORM EPOLLRDNORM
    EPOLLRDBAND = 0x080,
#define EPOLLRDBAND EPOLLRDBAND
    EPOLLWRNORM = 0x100,
#define EPOLLWRNORM EPOLLWRNORM
    EPOLLWRBAND = 0x200,
#define EPOLLWRBAND EPOLLWRBAND
    EPOLLMSG = 0x400,
#define EPOLLMSG EPOLLMSG
    EPOLLERR = 0x008,
#define EPOLLERR EPOLLERR
    EPOLLHUP = 0x010,
#define EPOLLHUP EPOLLHUP
    EPOLLRDHUP = 0x2000,
#define EPOLLRDHUP EPOLLRDHUP
    EPOLLONESHOT = (1 << 30),
#define EPOLLONESHOT EPOLLONESHOT
    EPOLLET = (1 << 31)
#define EPOLLET EPOLLET


#define EPOLL_CTL_ADD 1	
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3
*/
