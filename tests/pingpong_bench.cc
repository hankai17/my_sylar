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
                  //SYLAR_LOG_DEBUG(g_logger) << "flower_sta: " << flower_start;
              }
              ::send(g_pipes[2 * widx + 1], "m", 1, 0);
              //SYLAR_LOG_DEBUG(g_logger) << "fd: " << g_pipes[idx * 2] << " read callback " << ", send fd: "
                                        //<< g_pipes[2 * widx + 1];
              g_writes--;
          }
          g_fired++;
          sylar::IOManager::GetThis()->delEvent(g_pipes[idx * 2], sylar::IOManager::READ);
          //close(g_pipes[idx * 2]); // "close()" consume more time!
          //close(g_pipes[idx * 2 + 1]);
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

    //sylar::IOManager io(1, false, "iomanager", true); // 200ms too slow
    sylar::IOManager io(1, false, "iomanager"); // -O0 150ms // -O3 90ms
    //sleep(2);
    SYLAR_LOG_DEBUG(g_logger) << "schedu: " << sylar::GetCurrentUs();
    for (int i = 0; i < numPipes; ++i) {
      io.schedule(std::bind(fd_test, i));
    }
    /*
    sleep(5); //
    for (int i = 0; i < numActive; i++, g_fired++) {
        int ret = send(g_pipes[i * space + 1], "e", 1, 0);
        g_fired++;
        SYLAR_LOG_DEBUG(g_logger) << "init send fd: " << g_pipes[i * space + 1] << ", ret: " << ret;
    }
     */
    io.stop();
    return 0;
}



/*
 * Jemalloc so stable: always 60ns. However malloc always 60ns and 90ns
 *
[root@localhost cmake-build-debug]# ldd pingpong_test
linux-vdso.so.1 =>  (0x00007ffdd2d08000)
libmy_sylar.so => /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so (0x00007f8fdc8f7000)
libpthread.so.0 => /lib64/libpthread.so.0 (0x0000003e43600000)
libyaml-cpp.so.0.6 => /usr/local/lib/libyaml-cpp.so.0.6 (0x00007f8fdc660000)
libdl.so.2 => /lib64/libdl.so.2 (0x0000003e43a00000)
libssl.so.10 => /usr/lib64/libssl.so.10 (0x0000003e53600000)
libcrypto.so.10 => /usr/lib64/libcrypto.so.10 (0x0000003e4f200000)
libjemalloc.so.2 => /usr/local/lib/libjemalloc.so.2 (0x00007f8fdc1be000)
libstdc++.so.6 => /usr/local/lib64/libstdc++.so.6 (0x00007f8fdbe26000)
libm.so.6 => /lib64/libm.so.6 (0x0000003e44200000)
libgcc_s.so.1 => /usr/local/lib64/libgcc_s.so.1 (0x00007f8fdbc10000)
libc.so.6 => /lib64/libc.so.6 (0x0000003e43200000)
/lib64/ld-linux-x86-64.so.2 (0x0000003e42e00000)
libgssapi_krb5.so.2 => /lib64/libgssapi_krb5.so.2 (0x0000003e4fe00000)
libkrb5.so.3 => /lib64/libkrb5.so.3 (0x0000003e4fa00000)
libcom_err.so.2 => /lib64/libcom_err.so.2 (0x0000003e4ee00000)
libk5crypto.so.3 => /lib64/libk5crypto.so.3 (0x0000003e50600000)
libz.so.1 => /lib64/libz.so.1 (0x0000003e44600000)
librt.so.1 => /lib64/librt.so.1 (0x0000003e43e00000)
libkrb5support.so.0 => /lib64/libkrb5support.so.0 (0x0000003e50e00000)
libkeyutils.so.1 => /lib64/libkeyutils.so.1 (0x0000003e4f600000)
libresolv.so.2 => /lib64/libresolv.so.2 (0x0000003e45200000)
libselinux.so.1 => /lib64/libselinux.so.1 (0x0000003e44e00000)
[root@localhost cmake-build-debug]# ldd pingpong_test
linux-vdso.so.1 =>  (0x00007ffe089cb000)
libmy_sylar.so => /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so (0x00007ff2eb694000)
libpthread.so.0 => /lib64/libpthread.so.0 (0x0000003e43600000)
libyaml-cpp.so.0.6 => /usr/local/lib/libyaml-cpp.so.0.6 (0x00007ff2eb3fd000)
libdl.so.2 => /lib64/libdl.so.2 (0x0000003e43a00000)
libssl.so.10 => /usr/lib64/libssl.so.10 (0x0000003e53600000)
libcrypto.so.10 => /usr/lib64/libcrypto.so.10 (0x0000003e4f200000)
libstdc++.so.6 => /usr/local/lib64/libstdc++.so.6 (0x00007ff2eb065000)
libm.so.6 => /lib64/libm.so.6 (0x0000003e44200000)
libgcc_s.so.1 => /usr/local/lib64/libgcc_s.so.1 (0x00007ff2eae4e000)
libc.so.6 => /lib64/libc.so.6 (0x0000003e43200000)
/lib64/ld-linux-x86-64.so.2 (0x0000003e42e00000)
libgssapi_krb5.so.2 => /lib64/libgssapi_krb5.so.2 (0x0000003e4fe00000)
libkrb5.so.3 => /lib64/libkrb5.so.3 (0x0000003e4fa00000)
libcom_err.so.2 => /lib64/libcom_err.so.2 (0x0000003e4ee00000)
libk5crypto.so.3 => /lib64/libk5crypto.so.3 (0x0000003e50600000)
libz.so.1 => /lib64/libz.so.1 (0x0000003e44600000)
libkrb5support.so.0 => /lib64/libkrb5support.so.0 (0x0000003e50e00000)
libkeyutils.so.1 => /lib64/libkeyutils.so.1 (0x0000003e4f600000)
libresolv.so.2 => /lib64/libresolv.so.2 (0x0000003e45200000)
libselinux.so.1 => /lib64/libselinux.so.1 (0x0000003e44e00000)
        */
