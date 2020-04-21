#include "log.hh"
#include "iomanager.hh"
#include "stream.hh"
#include "uri.hh"
#include "tcp_server.hh"
#include "http/http_connection.hh"
#include <signal.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class TcpProxy : public sylar::TcpServer {
public:
    typedef std::shared_ptr<TcpProxy> ptr;
    TcpProxy(sylar::IOManager* worker = sylar::IOManager::GetThis(),
            sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
protected:
    virtual void handleClient(sylar::Socket::ptr client);
private:

};

TcpProxy::TcpProxy(sylar::IOManager *worker, sylar::IOManager *accept_worker)
: TcpServer(worker, accept_worker) {
}

static void shuttleData(sylar::Stream::ptr oneEnd, sylar::Stream::ptr otherEnd) {
    try {
        sylar::TransferStream(*oneEnd.get(), *otherEnd.get());
        int tmp_fd = std::dynamic_pointer_cast<sylar::SocketStream>(otherEnd)->getSocket()->getSocket();

        //otherEnd->close(); // 这里不close会导致sigpipe
        std::dynamic_pointer_cast<sylar::SocketStream>(otherEnd)->shutdown(SHUT_WR);

        SYLAR_LOG_DEBUG(g_logger) << "oneEnd closed, oneEnd fd: "
        << std::dynamic_pointer_cast<sylar::SocketStream>(oneEnd)->getSocket()->getSocket()
        << " otherEnd close, fd: " << tmp_fd;
    } catch (std::exception &) {
        SYLAR_LOG_DEBUG(g_logger) << "shuttleData failed";
    }
}

static void connectThem(sylar::Stream::ptr oneEnd, sylar::Stream::ptr otherEnd) {
    sylar::Scheduler* scheduler = sylar::Scheduler::GetThis();
    scheduler->schedule(std::bind(&shuttleData, oneEnd, otherEnd)); // 这里引用计数剧增! 从而导致即使shuttleData结束 也不会释放stream // 是好事儿 还是坏事儿
    scheduler->schedule(std::bind(&shuttleData, otherEnd, oneEnd));
    //SYLAR_LOG_DEBUG(g_logger) << "+++++++++oneEnd.use_count: " << oneEnd.use_count()
    //<< "  otherEnd.use_count: " << otherEnd.use_count();
}

std::string to_hex(const std::string& str) {
    std::stringstream ss;
    for(size_t i = 0; i < str.size(); ++i) {
        ss << std::setw(2) << std::setfill('0') << std::hex
           << (int)(uint8_t)str[i];
    }
    return ss.str();
}

void TcpProxy::handleClient(sylar::Socket::ptr client) {
    SYLAR_LOG_DEBUG(g_logger) << "start";

    sylar::Stream::ptr cs(new sylar::SocketStream(client));
    sylar::Stream::ptr ss = nullptr;
    if (getName() == "p1") {
        ss = tunnel(cs, "0.0.0.0", 1966);
    } else if (getName() == "p2") {
        ss = tunnel(cs);
    }
    if (ss == nullptr) {
        SYLAR_LOG_DEBUG(g_logger) << "tunnel return ss nullptr";
        return;
    } 
    connectThem(cs, ss);
    return;
}

void test_p2() {
    sylar::TcpServer::ptr proxy(new TcpProxy);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("0.0.0.0", 1967);
    proxy->bind(addr);
    proxy->setName("p2");
    proxy->start();
}

void test_p1() {
    sylar::TcpServer::ptr proxy(new TcpProxy);
    sylar::IPAddress::ptr addr = sylar::IPv4Address::Create("0.0.0.0", 2048);
    proxy->bind(addr);
    proxy->setName("p1");
    proxy->start();
}


// 手动调close时 仍有read事件
// 我们知道 这种场景下 手动close会产生一个fiber挂es队列上 然后关闭内核fd
// 如果此时又起一个socket恰是这个fd 且对此fd进行io操作 add到树上 必然报错
// tunnel场景: 
//        read到c的fin  返回出来手动关闭s
//        另一个方向 c<----s  s可能还在全局队列上 监听读事件 
//        由于请求很猛 又快速起一个该fd ...
// 具体分析一下 cstream sstream的生命周期
// 该怎么处理这种场景?  该如何理解socket的生命周期 stream的生命周期?

// 1 不允许主动调close  close必须放在socket/stream析构中调用  也就是说必须一口气初始化stream 
// 而且不能把socket暴露出来  也就是说close时机只有当stream引用计数完毕自动析构 其它一概不准 
// 果然 下载大文件时 即使客户端关闭了 另一端stream仍然再下 根本原因是WriteOne设计时 不像readOne那样有个失败判断
// 这里抓包很有意思 当向src发数据时 程序收到rst 就看不到发向src的包了 netstat也看不到任何状态 没有cw 但是程序依然保留着fd
// 所以这就是半关闭的应用场景了
// 2怎么支持半关闭?
void test1_1(sylar::Socket::ptr sock) {
    std::string buf;
    buf.resize(1024);

    sock->recv(&buf[0], buf.size());
    SYLAR_LOG_DEBUG(g_logger) << "end test1_1";
}

void test1() {
    //sylar::IPAddress::ptr addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80"); // 不知道为什么加上以后就变成阻塞的了
    sylar::IPAddress::ptr addr = sylar::IPAddress::Create("182.61.200.6", 80);
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

    sylar::IOManager::GetThis()->schedule(std::bind(test1_1, sock));
    SYLAR_LOG_DEBUG(g_logger) << "after schedule...";

    sleep(5);
    sock->close();
    SYLAR_LOG_DEBUG(g_logger) << "after sock->close: " << addr->toString();
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    sylar::IOManager iom(1, false, "io");
    iom.schedule(test_p1);
    iom.schedule(test_p2);
    //iom.schedule(test1);
    iom.stop();
    return 0; 
}

// 引用计数好吗?  不好 因为他模糊了对象生命周期 而在代码设计时 对象生命周期是根本核心
// 用引用计数的前提是  你得对这个对象的生命周期非常清晰

/*
2020-04-17 10:16:41	63640	1908	[DEBUG]	[system]	stream.cc:291	read src fin, fd: 13 totalRead: 514
2020-04-17 10:16:41	63640	1908	[DEBUG]	[system]	hook.cc:363	hook close fd: 14 ret: 1
2020-04-17 10:16:41	63640	1908	[DEBUG]	[root]	/root/CLionProjects/my_sylar/tests/tunnel_test.cc:31	oneEnd closed, oneEnd fd: 13 otherEnd close, fd: 14
2020-04-17 10:16:41	63640	1958	[DEBUG]	[system]	stream.cc:287	ParallelDo: 2473 2474
2020-04-17 10:16:41	63640	2006	[DEBUG]	[system]	stream.cc:287	ParallelDo: 2261 2262
2020-04-17 10:16:41	63640	2646	[DEBUG]	[root]	/root/CLionProjects/my_sylar/tests/tunnel_test.cc:61	start
2020-04-17 10:16:41	63640	2647	[DEBUG]	[root]	/root/CLionProjects/my_sylar/tests/tunnel_test.cc:61	start
2020-04-17 10:16:41	63640	2643	[DEBUG]	[system]	stream.cc:27	WriteOne 305
2020-04-17 10:16:41	63640	2645	[DEBUG]	[system]	stream.cc:27	WriteOne 1235
2020-04-17 10:16:41	63640	2628	[DEBUG]	[system]	stream.cc:61	new stream, cstream->fd: 38 sstream->fd: 54
2020-04-17 10:16:41	63640	2648	[DEBUG]	[system]	stream.cc:23	ReadOne result: 29
2020-04-17 10:16:41	63640	2652	[DEBUG]	[system]	stream.cc:27	WriteOne 29
2020-04-17 10:16:41	63640	2646	[DEBUG]	[system]	stream.cc:61	new stream, cstream->fd: 7 sstream->fd: 14
2020-04-17 10:16:41	63640	2646	[DEBUG]	[system]	iomanager.cc:125	addEvent assert fd = 14 event = 1
2020-04-17 10:16:41	63640	2646	[DEBUG]	[root]	iomanager.cc:127	ASSERTION: !(fd_ctx->events & event)
backtrace:
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZN5sylar9IOManager8addEventEiNS0_5EventESt8functionIFvvEE+0x46d) [0x7f7ede0b8bc3]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(+0x2c434b) [0x7f7ede0cc34b]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(recv+0x4d) [0x7f7ede0c89ed]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZN5sylar6Socket4recvEPvmi+0x40) [0x7f7ede0dd720]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZN5sylar12SocketStream4readEPvm+0x5b) [0x7f7ede1149ef]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZN5sylar6Stream11readFixSizeEPvm+0x51) [0x7f7ede114263]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZN5sylar6tunnelESt10shared_ptrINS_6StreamEERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEt+0xc29) [0x7f7ede110cd3]
    ./tunnel_test(_ZN8TcpProxy12handleClientESt10shared_ptrIN5sylar6SocketEE+0x1e8) [0x40cbb0]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZSt13__invoke_implIvRMN5sylar9TcpServerEFvSt10shared_ptrINS0_6SocketEEERS2_IS1_EJRS4_EET_St21__invoke_memfun_derefOT0_OT1_DpOT2_+0x98) [0x7f7ede10fa18]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZSt8__invokeIRMN5sylar9TcpServerEFvSt10shared_ptrINS0_6SocketEEEJRS2_IS1_ERS4_EENSt15__invoke_resultIT_JDpT0_EE4typeEOSC_DpOSD_+0x57) [0x7f7ede10f829]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZNSt5_BindIFMN5sylar9TcpServerEFvSt10shared_ptrINS0_6SocketEEES2_IS1_ES4_EE6__callIvJEJLm0ELm1EEEET_OSt5tupleIJDpT0_EESt12_Index_tupleIJXspT1_EEE+0x75) [0x7f7ede10f5d5]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZNSt5_BindIFMN5sylar9TcpServerEFvSt10shared_ptrINS0_6SocketEEES2_IS1_ES4_EEclIJEvEET0_DpOT_+0x2a) [0x7f7ede10f228]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZNSt17_Function_handlerIFvvESt5_BindIFMN5sylar9TcpServerEFvSt10shared_ptrINS2_6SocketEEES4_IS3_ES6_EEE9_M_invokeERKSt9_Any_data+0x20) [0x7f7ede10edd3]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZNKSt8functionIFvvEEclEv+0x32) [0x7f7ede0a6cec]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(_ZN5sylar5Fiber8MainFuncEl+0x26c) [0x7f7ede0abece]
    /root/CLionProjects/my_sylar/cmake-build-debug/libmy_sylar.so(make_fcontext+0x21) [0x7f7ede17d991]
*/
