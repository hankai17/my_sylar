#ifndef __TCP_SERVER_HH__
#define __TCP_SERVER_HH__

#include <memory>
#include <vector>
#include <string>
#include "nocopy.hh"
#include "socket.hh"
#include "iomanager.hh"
#include "address.hh"

namespace sylar {
class TcpServer : public std::enable_shared_from_this<TcpServer>,
        Nocopyable {
public:
    typedef std::shared_ptr<TcpServer> ptr;
    TcpServer(IOManager* worker = IOManager::GetThis(),
            IOManager* accept_worker = IOManager::GetThis());
    virtual ~TcpServer();
    virtual bool bind(Address::ptr addr);
    virtual bool bind(const std::vector<Address::ptr>& addrs,
            std::vector<Address::ptr>& fails);
    virtual bool start();
    virtual void stop();

    uint64_t getRecvTimeout() const { return m_recvTimeout; }
    void setRecvTimeout(uint64_t v) { m_recvTimeout = v; }
    std::string getName() const { return m_name; }
    void setName(const std::string& v) { m_name= v; }
    bool isStop() { return m_isStop; }

protected:
    virtual void handleClient(Socket::ptr client); // 因为是protect所以外部不能用  这里用protect的目的是为了让继承类调这两个函数的
    virtual void startAccept(Socket::ptr sock);

private:
    std::vector<Socket::ptr>     m_socks;
    IOManager*                   m_worker;
    IOManager*                   m_acceptWorker;
    uint64_t                     m_recvTimeout;
    std::string                  m_name;
    bool                         m_isStop;
};

}

#endif
