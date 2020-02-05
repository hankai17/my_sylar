#include "log.hh"
#include "iomanager.hh"
#include "hook.hh"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_sleep() {
    sylar::IOManager* io = sylar::IOManager::GetThis();
    SYLAR_LOG_DEBUG(g_logger) << "start test_sleep";
    io->schedule([](){
        sleep(2);
        SYLAR_LOG_DEBUG(g_logger) << "sleep 2";
    });

    io->schedule([](){
        sleep(3);
        SYLAR_LOG_DEBUG(g_logger) << "sleep 3";
    });
    SYLAR_LOG_DEBUG(g_logger) << "end test_sleep";
}

void test_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "111.206.176.91", &addr.sin_addr.s_addr); // www.ifeng.com

    SYLAR_LOG_DEBUG(g_logger) << "begin connect";
    int ret = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    SYLAR_LOG_DEBUG(g_logger) << "connect ret: " << ret << " errno: " << errno;
    if (ret) {
        return;
    }

    const char data[] = "GET / HTTP/1.1\r\n\r\n";
    //const char data[] = "GET / HTTP/1.0\r\nHost: www.ifeng.com\r\n\r\n";
    ret = send(sock, data, sizeof(data), 0);
    SYLAR_LOG_DEBUG(g_logger) << "send ret: " << ret << " errno: " << errno;
    if (ret <= 0) {
        return;
    }

    std::string buf;
    buf.resize(4096);
    ret = recv(sock, &buf[0], buf.size(), 0);
    SYLAR_LOG_DEBUG(g_logger) << "recv ret: " << ret << " errno: " << errno;
    if (ret <= 0)  {
        return;
    }

    buf.resize(ret);
    SYLAR_LOG_DEBUG(g_logger) << buf;
    close(sock);
}

int main() {
    sylar::IOManager iom(1, false);
    //iom.schedule(test_sleep);
    iom.schedule(test_socket);
    iom.stop();
    return 0;
}
