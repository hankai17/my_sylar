#include "iomanager.hh"
#include "log.hh"

#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <functional>

std::vector<int> g_pipes;
int numPipes;
int numActive;
int numWrites;
int g_reads, g_writes, g_fired;
sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void fd_test(int fd) {
    //SYLAR_LOG_DEBUG(g_logger) << "fd: " << fd;
    sylar::IOManager::GetThis()->addEvent(fd, sylar::IOManager::READ, [=]() {
        char ch;
        g_reads += static_cast<int>(::recv(fd, &ch, sizeof(ch), 0));
        if (g_writes > 0) {
          int widx = fd + 1;
          if (widx >= numPipes) {
            widx -= numPipes;
          }
          ::send(g_pipes[2 * widx + 1], "m", 1, 0);
          SYLAR_LOG_DEBUG(g_logger) << "fd: "<< fd << " read callback " << ", send fd: " << g_pipes[2 * widx + 1];
          g_writes--;
          g_fired++;
        }
        if (g_fired == g_reads) {
            SYLAR_LOG_DEBUG(g_logger) << "g_fired == g_reads";
        }

    });

}

int main(int argc, char* argv[]) {
    numPipes = 100;
    numActive = 1;
    numWrites = 100;

    g_fired = numActive;
    g_reads = 0;
    g_writes = numWrites;

    int c;
    while ((c = getopt(argc, argv, "n:a:w:")) != -1) {
      switch (c) {
        case 'n':
          numPipes = atoi(optarg);
          break;
        case 'a':
          numActive = atoi(optarg);
          break;
        case 'w':
          numWrites = atoi(optarg);
          break;
        default:
          fprintf(stderr, "Illegal argument \"%c\"\n", c);
          return 1;
      }
    }

    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = numPipes * 2 + 50;
    if (::setrlimit(RLIMIT_NOFILE, &rl) == -1) {
      perror("setrlimit");
      //return 1;  // comment out this line if under valgrind
    }
    g_pipes.resize(2 * numPipes);
    SYLAR_LOG_DEBUG(g_logger) << "numPipes: " << numPipes << ", numActive: " << numActive 
      << ", numWrites: " << numWrites;

    for (int i = 0; i < numPipes; ++i) {
      if (::socketpair(AF_UNIX, SOCK_STREAM, 0, &g_pipes[i*2]) == -1) {
        perror("pipe");
        return 1;
      }
    }

    for (int i = 0; i < numPipes * 2; ++i) {
        SYLAR_LOG_DEBUG(g_logger) << "g_pipes[" << i << "]: " << g_pipes[i];
    }

    int space = numPipes / numActive;
    space *= 2;
    for (int i = 0; i < numActive; i++, g_fired++) {
        int ret = send(g_pipes[i * space + 1], "e", 1, 0);
        SYLAR_LOG_DEBUG(g_logger) << "send fd: " << g_pipes[i * space + 1] << ", send ret: " << ret;
    }
      
    sylar::IOManager io(1, false, "iomanager");

    for (int i = 0; i < numPipes; ++i) {
      io.schedule(std::bind(fd_test, g_pipes[i*2]));
    }
    io.stop();
    return 0;
}

