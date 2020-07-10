#include "my_sylar/log.hh"
#include "my_sylar/uri.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/address.hh"
#include "my_sylar/socket.hh"
#include "my_sylar/util.hh"
#include "my_sylar/ns/ares.hh"
#include "my_sylar/http/http_connection.hh"

#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/time.h>
#include<arpa/inet.h>
#include<netinet/tcp.h>
#include <sys/socket.h>

#define closesocket close

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
sylar::AresChannel::ptr channel = nullptr;

void test() {
    //sylar::Uri::ptr uri = sylar::Uri::Create("http://www.ifeng.com/path/to/1.html?k=v&k1=v1#frag=1");
    sylar::Uri::ptr uri = sylar::Uri::Create("https://www.ifeng.com/path/to/瓜枣/1.html?k=v&k1=v1&k2=瓜枣#frag=瓜枣");
    SYLAR_LOG_DEBUG(g_logger) << "toString: " << uri->toString();

    SYLAR_LOG_DEBUG(g_logger) << "uifo: " << uri->getUserinfo();
    SYLAR_LOG_DEBUG(g_logger) << "sche: " << uri->getScheme();
    SYLAR_LOG_DEBUG(g_logger) << "host: " << uri->getHost();
    SYLAR_LOG_DEBUG(g_logger) << "port: " << uri->getPort();
    SYLAR_LOG_DEBUG(g_logger) << "path: " << uri->getPath();
    SYLAR_LOG_DEBUG(g_logger) << "qery: " << uri->getQuery();
    SYLAR_LOG_DEBUG(g_logger) << "frag: " << uri->getFragment();

    auto addr = uri->createAddress();
    SYLAR_LOG_DEBUG(g_logger) << addr->toString();
    return;
}

void test_gethostbyname(int idx) {
    struct hostent* hptr;
    std::string dom = "www.ifeng.com";
    const char* ptr = dom.c_str();
    char** pptr;
    char   str[32];
 
    SYLAR_LOG_DEBUG(g_logger) << "idx: " << idx << " start";
    if((hptr = gethostbyname(ptr)) == NULL) {
        printf(" gethostbyname error for host:%s\n", ptr);
        return;
    }
 
    printf("official hostname:%s\n",hptr->h_name);
    for(pptr = hptr->h_aliases; *pptr != NULL; pptr++)
        printf(" alias:%s\n",*pptr);
 
    switch(hptr->h_addrtype) {
        case AF_INET:
        case AF_INET6:
            pptr=hptr->h_addr_list;
            for(; *pptr!=NULL; pptr++)
                printf(" address:%s\n",
                       inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str)));
            printf(" first address: %s\n",
                       inet_ntop(hptr->h_addrtype, hptr->h_addr, str, sizeof(str)));
        break;
        default:
            printf("unknown address type\n");
        break;
    }
    SYLAR_LOG_DEBUG(g_logger) << "idx: " << idx << " end";
    return;
}

void test_req(const std::string& req) {
    sylar::Uri::ptr uri = sylar::Uri::Create(req.c_str());
    if (uri == nullptr) {
        //SYLAR_LOG_DEBUG(g_logger) << "Can not parse req: " << req;
        return;
    }
    //SYLAR_LOG_DEBUG(g_logger) << "req start: " << req;

    sylar::http::HttpResult::ptr resu =  sylar::http::HttpConnection::DoGet(uri, 1000 * 5);
    //SYLAR_LOG_DEBUG(g_logger) << resu->toString();
    //SYLAR_LOG_DEBUG(g_logger) << resu->response->getHeader("Content-Length", "null");
    if (resu == nullptr || resu->response == nullptr) {
        //SYLAR_LOG_DEBUG(g_logger) << "Cannot get result may be timeout " << req;
        return;
    }
    std::string str_flow = resu->response->getHeader("Content-Length", "null");
    SYLAR_LOG_DEBUG(g_logger) << "req end: " << str_flow << " " << req;

    return;
}

void test1() {
    sylar::IPAddress::ptr addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80");
    if (addr) {
        SYLAR_LOG_DEBUG(g_logger) << "addr: " << addr->toString();
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "get addr fail";
        return;;
    }
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect faild addr: " << addr->toString();
        return;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "connect succeed: " << addr->toString();
    }

    uint64_t ts = sylar::GetCurrentUs();
    for (size_t i = 0; i < 10000000000ul; i++) {
        if (int err = sock->getError()) {
            SYLAR_LOG_DEBUG(g_logger) << "err: " << err
            << " strerror: " << strerror(errno);
            break;
        }

        struct tcp_info tcp_info;
        if(!sock->getOption(IPPROTO_TCP, TCP_INFO, tcp_info)) {
            SYLAR_LOG_INFO(g_logger) << "err";
            break;
        }
        if(tcp_info.tcpi_state != TCP_ESTABLISHED) {
            SYLAR_LOG_INFO(g_logger)
                    << " state=" << (int)tcp_info.tcpi_state;
            break;
        }
        static int batch = 10000000;
        if(i && (i % batch) == 0) {
            uint64_t ts2 = sylar::GetCurrentUs();
            SYLAR_LOG_INFO(g_logger) << "i=" << i << " used: " << ((ts2 - ts) * 1.0 / batch) << " us";
            ts = ts2;
        }
    }
}

void test3() {
    //sylar::IPAddress::ptr addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80");
    sylar::IPAddress::ptr addr = sylar::IPAddress::Create("61.135.169.121", 80);
    if (addr) {
        SYLAR_LOG_DEBUG(g_logger) << "addr: " << addr->toString();
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "get addr fail";
        return;;
    }
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "connect faild addr: " << addr->toString();
        return;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "connect succeed: " << addr->toString();
    }

    //uint64_t ts = sylar::GetCurrentUs();
}

void test1_1(sylar::Socket::ptr sock) {
    std::string buf;
    buf.resize(1024);

    sock->recv(&buf[0], buf.size());
    SYLAR_LOG_DEBUG(g_logger) << "end test1_1";
}

void test2() {
    sylar::IPAddress::ptr addr = nullptr;
    if (0) {
        addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80"); // 不知道为什么加上以后就变成阻塞的了
    } else {
        std::string domain("www.ifeng.com");
        if (channel == nullptr) {
            SYLAR_LOG_DEBUG(g_logger) << "channel is nullptr sleep 5s...";
            sleep(5);
        }
        auto ips = channel->aresGethostbyname(domain.c_str());
        for (auto& i : ips) {
            SYLAR_LOG_DEBUG(g_logger) << i.toString();
        }
        SYLAR_LOG_DEBUG(g_logger) << "domain: " << domain << ", test done";

        addr = std::dynamic_pointer_cast<sylar::IPAddress>(
              sylar::Address::Create(ips[0].getAddr(), ips[0].getAddrLen())
              );
    }

    if (addr) {
        SYLAR_LOG_DEBUG(g_logger) << "addr: " << addr->toString();
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "get addr fail";
        return;;
    }
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        SYLAR_LOG_DEBUG(g_logger) << "fd: " << sock->getSocket() << " connect faild addr: " << addr->toString();
        return;
    } else {
        SYLAR_LOG_DEBUG(g_logger) << "fd: " << sock->getSocket() << " connect succeed: " << addr->toString();
    }

    sylar::IOManager::GetThis()->schedule(std::bind(test1_1, sock));
    SYLAR_LOG_DEBUG(g_logger) << "after schedule...";

    sleep(10);
    sock->close(); //这里手动close 底层会触发test_1中的读事件 (so dangerous!)
    SYLAR_LOG_DEBUG(g_logger) << "after sock->close: " << addr->toString();
}

void ares_test() {
    SYLAR_LOG_DEBUG(g_logger) << "in ares test";
    channel.reset(new sylar::AresChannel);
    channel->init();
    channel->start();
}

int main() {
    sylar::IOManager iom(1, false, "io");
    //iom.schedule(test1);
    //iom.schedule(test2);
    //iom.schedule(ares_test);
    //iom.schedule(std::bind(test_req, "http://www.ifeng.com/"));
    iom.schedule(std::bind(test_req, "https://www.baidu.com/"));
    /*
    for (int i = 0; i < 10; i++) {
        iom.schedule(std::bind(test_gethostbyname, i));
    }
     */
    iom.stop();
    return 0;
}

/*
0  0x0000003e432df383 in poll () from /lib64/libc.so.6
#1  0x0000003e4520c086 in __libc_res_nsend () from /lib64/libresolv.so.2
#2  0x0000003e45208811 in __libc_res_nquery () from /lib64/libresolv.so.2
#3  0x0000003e45208dd0 in __libc_res_nquerydomain () from /lib64/libresolv.so.2
#4  0x0000003e452098f8 in __libc_res_nsearch () from /lib64/libresolv.so.2
#5  0x00007f2124742411 in _nss_dns_gethostbyname3_r () from /lib64/libnss_dns.so.2
#6  0x00007f212474265e in _nss_dns_gethostbyname_r () from /lib64/libnss_dns.so.2
#7  0x0000003e43303c67 in gethostbyname_r@@GLIBC_2.2.5 () from /lib64/libc.so.6
#8  0x0000003e43303483 in gethostbyname () from /lib64/libc.so.6
#9  0x000000000040f81d in test_gethostbyname(int) ()
#10 0x00000000004165c0 in _ZSt13__invoke_implIvRPFviEJRiEET_St14__invoke_otherOT0_DpOT1_ ()
#11 0x0000000000416457 in _ZSt8__invokeIRPFviEJRiEENSt15__invoke_resultIT_JDpT0_EE4typeEOS5_DpOS6_ ()
#12 0x0000000000416126 in _ZNSt5_BindIFPFviEiEE6__callIvJEJLm0EEEET_OSt5tupleIJDpT0_EESt12_Index_tupleIJXspT1_EEE ()
#13 0x0000000000415d64 in _ZNSt5_BindIFPFviEiEEclIJEvEET0_DpOT_ ()
#14 0x00000000004157c4 in std::_Function_handler<void ()(), std::_Bind<void (*()(int))(int)> >::_M_invoke(std::_Any_data const&) ()
#15 0x00007f212621f040 in std::function<void ()()>::operator()() const () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#16 0x00007f2126224172 in sylar::Fiber::MainFunc(long) () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#17 0x00007f21262f6bc1 in make_fcontext () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#18 0x0000000000000000 in ?? ()
Thread 1 (Thread 0x7f212576c860 (LWP 10244)):
#0  0x0000003e436082fd in pthread_join () from /lib64/libpthread.so.0
#1  0x00007f212621d4fc in sylar::Thread::join() () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#2  0x00007f212622c44f in sylar::Scheduler::stop() () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#3  0x0000000000412eb2 in main ()
 */

/*

Thread 2 (Thread 0x7f74f622e700 (LWP 10626)):
#0  0x0000003e432df383 in poll () from /lib64/libc.so.6
#1  0x0000003e4520c086 in __libc_res_nsend () from /lib64/libresolv.so.2
#2  0x0000003e45208811 in __libc_res_nquery () from /lib64/libresolv.so.2
#3  0x0000003e45208dd0 in __libc_res_nquerydomain () from /lib64/libresolv.so.2
#4  0x0000003e452098f8 in __libc_res_nsearch () from /lib64/libresolv.so.2
#5  0x00007f74f521b411 in _nss_dns_gethostbyname3_r () from /lib64/libnss_dns.so.2
#6  0x00007f74f521b6e4 in _nss_dns_gethostbyname2_r () from /lib64/libnss_dns.so.2
#7  0x0000003e43303915 in gethostbyname2_r@@GLIBC_2.2.5 () from /lib64/libc.so.6
#8  0x0000003e432d0d62 in gaih_inet () from /lib64/libc.so.6
#9  0x0000003e432d2fbf in getaddrinfo () from /lib64/libc.so.6
#10 0x00007f74f6d01590 in sylar::Address::Lookup(std::vector<std::shared_ptr<sylar::Address>, std::allocator<std::shared_ptr<sylar::Address> > >&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, int, int, int) () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#11 0x00007f74f6d01eaf in sylar::Address::LookupAnyIPAddress(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, int, int, int) () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#12 0x00007f74f6d8a5f1 in sylar::Uri::createAddress() const () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#13 0x00007f74f6d68817 in sylar::http::HttpConnection::DoRequest(std::shared_ptr<sylar::http::HttpRequest>, std::shared_ptr<sylar::Uri>, unsigned long) () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#14 0x00007f74f6d68703 in sylar::http::HttpConnection::DoRequest(sylar::http::HttpMethod, std::shared_ptr<sylar::Uri>, unsigned long, std::map<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::less<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >, std::allocator<std::pair<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > > const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&) () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#15 0x00007f74f6d6807d in sylar::http::HttpConnection::DoGet(std::shared_ptr<sylar::Uri>, unsigned long, std::map<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::less<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >, std::allocator<std::pair<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > > const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&) () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#16 0x000000000041041c in test_req(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&) ()
#17 0x0000000000416d3f in _ZSt13__invoke_implIvRPFvRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEEJRPKcEET_St14__invoke_otherOT0_DpOT1_ ()
#18 0x0000000000416bb1 in _ZSt8__invokeIRPFvRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEEJRPKcEENSt15__invoke_resultIT_JDpT0_EE4typeEOSF_DpOSG_ ()
#19 0x0000000000416872 in _ZNSt5_BindIFPFvRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEEPKcEE6__callIvJEJLm0EEEET_OSt5tupleIJDpT0_EESt12_Index_tupleIJXspT1_EEE ()
#20 0x00000000004164b0 in _ZNSt5_BindIFPFvRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEEPKcEEclIJEvEET0_DpOT_ ()
#21 0x0000000000415f1a in std::_Function_handler<void ()(), std::_Bind<void (*()(char const*))(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)> >::_M_invoke(std::_Any_data const&) ()
#22 0x00007f74f6ce5010 in std::function<void ()()>::operator()() const () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#23 0x00007f74f6cea142 in sylar::Fiber::MainFunc(long) () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#24 0x00007f74f6dbcb91 in make_fcontext () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#25 0x0000000000000000 in ?? ()
Thread 1 (Thread 0x7f74f6232860 (LWP 10625)):
#0  0x0000003e436082fd in pthread_join () from /lib64/libpthread.so.0
#1  0x00007f74f6ce34cc in sylar::Thread::join() () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#2  0x00007f74f6cf241f in sylar::Scheduler::stop() () from /root/CLionProjects/my_sylar/build/libmy_sylar.so
#3  0x000000000041361a in main ()


 */
