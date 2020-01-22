#include "iomanager.hh"
#include "log.hh"
#include "util.hh"

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
uint64_t flower_start = 0;
uint64_t flower_end = 0;

void fd_test(int idx) {
    sylar::IOManager::GetThis()->addEvent(g_pipes[idx * 2], sylar::IOManager::READ, [=]() {
        char ch;
        g_reads += static_cast<int>(::recv(g_pipes[idx * 2], &ch, sizeof(ch), 0));
        if (g_writes > 0) {
          int widx = idx + 1;
          if (widx >= numPipes) {
            //widx -= numPipes;
            //close
            flower_end = sylar::GetCurrentUs();
            SYLAR_LOG_DEBUG(g_logger) << "flower_end: " << flower_end;
            SYLAR_LOG_DEBUG(g_logger) << "elapse: " << flower_end - flower_start;
          } else {
              if (idx == 0) {
                  flower_start = sylar::GetCurrentUs();
                  SYLAR_LOG_DEBUG(g_logger) << "flower_sta: " << flower_start;
              }
              ::send(g_pipes[2 * widx + 1], "m", 1, 0);
              //SYLAR_LOG_DEBUG(g_logger) << "fd: " << g_pipes[idx * 2] << " read callback " << ", send fd: "
                                        //<< g_pipes[2 * widx + 1];
              g_writes--;
          }
          g_fired++;
          sylar::IOManager::GetThis()->delEvent(g_pipes[idx * 2], sylar::IOManager::READ);
          close(g_pipes[idx * 2]);
          close(g_pipes[idx * 2 + 1]);
          //SYLAR_LOG_DEBUG(g_logger) << "delEvent fd: " << g_pipes[idx * 2] << "  close it and " << g_pipes[idx * 2 + 1];
        }
    });

}

int main(int argc, char* argv[]) {
    numPipes = 10000;
    numActive = 1;
    numWrites = 10000; // result 150ms however libevent2 15ms. 10 Times slow
    g_fired = 0; // Sum send bytes

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
    g_writes = numWrites; // Sum Write count

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
        //SYLAR_LOG_DEBUG(g_logger) << "g_pipes[" << i << "]: " << g_pipes[i];
    }

    int space = numPipes / numActive;
    space *= 2;
    for (int i = 0; i < numActive; i++, g_fired++) {
        int ret = send(g_pipes[i * space + 1], "e", 1, 0);
        g_fired++;
        SYLAR_LOG_DEBUG(g_logger) << "init send fd: " << g_pipes[i * space + 1] << ", ret: " << ret;
    }
      
    sylar::IOManager io(1, false, "iomanager");
    //sleep(2);
    SYLAR_LOG_DEBUG(g_logger) << "schedu: " << sylar::GetCurrentUs();
    for (int i = 0; i < numPipes; ++i) {
      io.schedule(std::bind(fd_test, i));
    }
    io.stop();
    return 0;
}

