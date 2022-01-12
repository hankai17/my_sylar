#ifndef __HOOK_HH__
#define __HOOK_HH__

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

namespace sylar {
    bool is_hook_enable();
    void set_hook_enable(bool flag);
}

extern "C" {
    // sleep
    typedef unsigned int (*sleep_fun)(unsigned int seconds);
    extern sleep_fun sleep_f;
    extern unsigned int Sleep(unsigned int seconds);

    typedef int (*usleep_fun)(useconds_t usec);
    extern usleep_fun usleep_f;
    extern int Usleep(useconds_t usec);

    typedef int (*nanosleep_fun)(const struct timespec* req, struct timespec* rem);
    extern nanosleep_fun nanosleep_f;
    extern int Nanosleep(const struct timespec *req, struct timespec *rem);

    // socket
    typedef int (*socket_fun)(int domain, int type, int protocol);
    extern socket_fun socket_f;
    extern int Socket(int domain, int type, int protocol);

    typedef int (*connect_fun)(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
    extern connect_fun connect_f;
    extern int Connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
    extern int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms);

    typedef int (*accept_fun)(int s, struct sockaddr* addr, socklen_t* addrlen);
    extern accept_fun accept_f;
    extern int Accept(int s, struct sockaddr* addr, socklen_t* addrlen);

    //read
    typedef ssize_t (*read_fun)(int fd, void* buf, size_t count);
    extern read_fun read_f;
    extern ssize_t Read(int fd, void* buf, size_t count);

    typedef ssize_t (*readv_fun)(int fd, const struct iovec* iov, int iovcnt);
    extern readv_fun readv_f;
    extern ssize_t Readv(int fd, const struct iovec* iov, int iovcnt);

    typedef ssize_t (*recv_fun)(int sockfd, void* buf, size_t len, int flags);
    extern recv_fun recv_f;
    extern ssize_t Recv(int sockfd, void* buf, size_t len, int flags);

    typedef ssize_t (*recvfrom_fun)(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);
    extern recvfrom_fun recvfrom_f;
    extern ssize_t Recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);

    typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr* msg, int flags);
    extern recvmsg_fun recvmsg_f;
    extern ssize_t Recvmsg(int sockfd, struct msghdr* msg, int flags);

    // write
    typedef ssize_t (*write_fun)(int fd, const void* buf, size_t count);
    extern write_fun write_f;
    extern ssize_t Write(int fd, const void* buf, size_t count);

    typedef ssize_t (*writev_fun)(int fd, const struct iovec* iov, int iovcnt);
    extern writev_fun writev_f;
    extern ssize_t Writev(int fd, const struct iovec* iov, int iovcnt);

    typedef ssize_t (*send_fun)(int s, const void* msg, size_t len, int flags);
    extern send_fun send_f;
    extern ssize_t Send(int s, const void* msg, size_t len, int flags);

    typedef ssize_t (*sendto_fun)(int s, const void* msg, size_t len, int flags, const struct sockaddr* to, socklen_t tolen);
    extern sendto_fun sendto_f;
    extern ssize_t Sendto(int s, const void* msg, size_t len, int flags, const struct sockaddr* to, socklen_t tolen);

    typedef ssize_t (*sendmsg_fun)(int s, const struct msghdr* msg, int flags);
    extern sendmsg_fun sendmsg_f;
    extern ssize_t Sendmsg(int s, const struct msghdr *msg, int flags);

    // close
    typedef int (*close_fun)(int fd);
    extern close_fun close_f;
    extern int Close(int fd);

    typedef int (*closesocket_fun)(int fd);
    extern closesocket_fun closesocket_f;
    extern int Closesocket(int fd);

    typedef int (*shutdown_fun)(int fd, int how);
    extern shutdown_fun shutdown_f;
    extern int Shutdown(int fd, int how);

    // socket operation
    typedef int (*fcntl_fun)(int fd, int cmd, ... /* arg */ );
    extern fcntl_fun fcntl_f;
    extern int Fcntl(int fd, int cmd, ... /* arg */ );

    typedef int (*ioctl_fun)(int d, unsigned long int request, ...);
    extern ioctl_fun ioctl_f;
    extern int Ioctl(int d, unsigned long int request, ...);

    typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
    extern getsockopt_fun getsockopt_f;
    extern int Getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

    typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
    extern setsockopt_fun setsockopt_f;
    extern int Setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
}

#endif