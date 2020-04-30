#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <iostream>

#include "ares.hh"
#include "my_sylar/util.hh"
#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/timer.hh"

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    static char *try_config(char *s, const char *opt) {
        size_t len;
        len = strlen(opt);
        if (strncmp(s, opt, len) != 0 || !isspace((unsigned char) s[len]))
            return NULL;
        s += len;
        while (isspace((unsigned char) *s))
            s++;
        return s;
    }

    static int init_by_resolv_conf(AresChannel *channel) {
        std::map<int, std::map<sylar::Address::ptr, Socket::ptr> > servers;
        {
            char *p;
            std::string line;
            std::ifstream ifs;
            int i = 0;
            if (!sylar::FSUtil::OpenForRead(ifs, PATH_RESOLV_CONF, std::ios::binary)) {
                SYLAR_LOG_ERROR(g_logger) << "Can not open " << PATH_RESOLV_CONF << " errno: "
                                          << errno << " strerrno: " << strerror(errno);
                return 0;
            }
            while (getline(ifs, line)) {
                if ((p = try_config(const_cast<char *>(line.c_str()), "domain"))) {
                    continue;
                } else if ((p = try_config(const_cast<char *>(line.c_str()), "nameserver"))) {
                    servers[i].insert(std::make_pair(IPAddress::Create(p, 53), nullptr));
                    i++;
                }
            }
        }
        if (servers.size()) {
            channel->setServers(std::move(servers));
        }
        return 0;
    }
    ///////////////////////////////////////////////////////////////////////////

    int ServerState::openTcpSocket(uint16_t port) {
        int s, flags;
        struct sockaddr_in sockin;
        s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == -1)
            return -1;
        flags = fcntl(s, F_GETFL, 0);
        if (flags == -1) {
            close(s);
            return -1;
        }
        flags |= O_NONBLOCK;
        if (fcntl(s, F_SETFL, flags) == -1) {
            close(s);
            return -1;
        }
        /* Connect to the server. */
        memset(&sockin, 0, sizeof(sockin));
        sockin.sin_family = AF_INET;
        sockin.sin_addr = addr;
        sockin.sin_port = port;
        if (connect(s, (struct sockaddr *) &sockin, sizeof(sockin)) == -1 && errno != EINPROGRESS) {
            close(s);
            return -1;
        }
        tcp_socket = s;
        return 0;
    }

    int ServerState::openUdpSocket(uint16_t port) {
        int s;
        struct sockaddr_in sockin;
        /* Acquire a socket. */
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s == -1) return -1;
        /* Connect to the server. */
        memset(&sockin, 0, sizeof(sockin));
        sockin.sin_family = AF_INET;
        sockin.sin_addr = addr;
        sockin.sin_port = port;
        if (connect(s, (struct sockaddr *) &sockin, sizeof(sockin)) == -1) {
            close(s);
            return -1;
        }
        udp_socket = s;
        return 0;
    }

    AresChannel::AresChannel(IOManager *worker, IOManager *receiver)
            : UdpServer(worker, receiver) {
    }

    void AresChannel::init() {
        struct timeval tv;
        init_by_resolv_conf(this);
        if (m_servers.size()) {
            for (auto& i : m_servers) {
                auto &j = i.second;
                j.begin()->second = bind(nullptr);
            }
        }
        gettimeofday(&tv, NULL);
        m_nextId = (unsigned short) (tv.tv_sec ^ tv.tv_usec ^ getpid()) & 0xffff;
    }

    void AresChannel::nextServer(Query::ptr query) {
        query->server_idx++;
        for (; query->try_count < m_tries; query->try_count++) { // try小于全局定义的次数
            for (; query->server_idx < getServerSize(); query->server_idx++) {
                if (!query->skip_server[query->server_idx]) {
                    ares_send(query);
                    return;
                }
            }
            query->server_idx = 0;
            //if(query->using_tcp)
            //break;
        }
    }

    void AresChannel::ares_send(Query::ptr query) {
        auto server = m_servers[query->server_idx];
        auto it = server.begin();
        if (query->using_tcp) {
            if (it->second == nullptr) {
                it->second = bind(nullptr);
                if (false) {
                    query->skip_server[query->server_idx] = 1;
                    nextServer(query);
                    return;
                }
            }
            SendRequest::ptr sendreq(new SendRequest);
            sendreq->data = &query->m_tcpbuf[0];
            sendreq->len = query->m_tcpbuf.size();
            query->timeout = 0;
        } else {
            if (it->second == nullptr) {
                it->second = bind(nullptr);
                if (false) {
                    query->skip_server[query->server_idx] = 1;
                    nextServer(query);
                    return;
                }
            }
            if (it->second->sendTo(query->qbuf, query->qlen, it->first) == -1) {
                query->skip_server[query->server_idx] = 1;
                nextServer(query);
                return;
            }
            query->timeout =
                    time(0) + ((query->try_count == 0) ? m_timeout : m_timeout << query->try_count / getServerSize());
        }
    }

    int AresChannel::ares_mkquery(const char *name, int dnsclass, int type, uint16_t id, int rd,
                                  std::vector<uint8_t> &buf/*in-out*/) {
        int len;
        unsigned char *q;
        const char *p;

        /* Compute the length of the encoded name so we can check buflen.
         * Start counting at 1 for the zero-length label at the end. */
        len = 1;
        for (p = name; *p; p++) {
            if (*p == '\\' && *(p + 1) != 0) p++;
            len++;
        }
        /* If there are n periods in the name, there are n + 1 labels, and
         * thus n + 1 length fields, unless the name is empty or ends with a
         * period.  So add 1 unless name is empty or ends with a period.
         */
        if (*name && *(p - 1) != '.')  //顶级域必须有点结尾
            len++;

        buf.resize(len + HFIXEDSZ + QFIXEDSZ);

        /* Set up the header. */
        q = &buf[0];
        memset(q, 0, HFIXEDSZ);
        DNS_HEADER_SET_QID(q, id);
        DNS_HEADER_SET_OPCODE(q, QUERY);
        DNS_HEADER_SET_RD(q, (rd) ? 1 : 0);
        DNS_HEADER_SET_QDCOUNT(q, 1);

        /* A name of "." is a screw case for the loop below, so adjust it. */
        if (strcmp(name, ".") == 0)
            name++;

        /* Start writing out the name after the header. */
        q += HFIXEDSZ;
        while (*name) {
            if (*name == '.') return ARES_EBADNAME;

            /* Count the number of bytes in this label. */
            len = 0;
            for (p = name; *p && *p != '.'; p++) {
                if (*p == '\\' && *(p + 1) != 0)
                    p++;
                len++;
            }
            if (len > MAXLABEL) return ARES_EBADNAME;

            /* Encode the length and copy the data. */
            *q++ = len;
            for (p = name; *p && *p != '.'; p++) {
                if (*p == '\\' && *(p + 1) != 0)
                    p++;
                *q++ = *p;
            }

            /* Go to the next label and repeat, unless we hit the end. */
            if (!*p) break;
            name = p + 1;
        }

        /* Add the zero-length label at the end. */
        *q++ = 0;

        /* Finish off the question with the type and class. */
        DNS_QUESTION_SET_TYPE(q, type);
        DNS_QUESTION_SET_CLASS(q, dnsclass);
        return ARES_SUCCESS;
    }

    static int name_length(const uint8_t *encoded, const uint8_t *abuf, int alen) { //返回压缩前 域名长度 包括点
        int n = 0, offset, indir = 0;

        if (encoded == abuf + alen) return -1;

        while (*encoded) {
            if ((*encoded & INDIR_MASK) == INDIR_MASK) { //0xc0
                /* Check the offset and go there. */
                if (encoded + 1 >= abuf + alen) return -1;
                offset = (*encoded & ~INDIR_MASK) << 8 | *(encoded + 1);
                if (offset >= alen) return -1;
                encoded = abuf + offset;

                /* If we've seen more indirects than the message length,
                 * then there's a loop.
                 */
                if (++indir > alen) return -1;
            } else {
                offset = *encoded;
                if (encoded + offset + 1 >= abuf + alen) return -1;
                encoded++;
                while (offset--) {
                    n += (*encoded == '.' || *encoded == '\\') ? 2 : 1;
                    encoded++;
                }
                n++;
            }
        }

        /* If there were any labels at all, then the number of dots is one
         * less than the number of labels, so subtract one.
         */
        return (n) ? n - 1 : n;
    }

    int AresChannel::aresExpandName(uint8_t *encoded, uint8_t *abuf, int alen,
                                    std::vector<uint8_t> &s, int *enclen) {
        int len, indir = 0;
        const uint8_t *p;

        len = name_length(encoded, abuf, alen); //仅是assert作用 感觉很多余?
        if (len == -1) return ARES_EBADNAME;

        s.resize(len + 1);
        uint8_t *q = &s[0];

        /* No error-checking necessary; it was all done by name_length(). */
        p = encoded;
        while (*p) { //拷贝
            if ((*p & INDIR_MASK) == INDIR_MASK) {
                if (!indir) {
                    *enclen = p + 2 - encoded;
                    indir = 1;
                }
                p = abuf + ((*p & ~INDIR_MASK) << 8 | *(p + 1));
            } else {
                len = *p;
                p++;
                while (len--) {
                    if (*p == '.' || *p == '\\')
                        *q++ = '\\';
                    *q++ = *p;
                    p++;
                }
                *q++ = '.';
            }
        }
        if (!indir)
            *enclen = p + 1 - encoded;

        /* Nuke the trailing period if we wrote one. */
        if (q > &s[0])
            *(q - 1) = 0;

        return ARES_SUCCESS;
    }

    Query::ptr AresChannel::aresSend(const std::vector<uint8_t> &qbuf) {
        int qlen = qbuf.size();
        if (qlen < HFIXEDSZ || qlen >= (1 << 16)) {
            SYLAR_LOG_ERROR(g_logger) << "aresSend qlen too small or too max " << qlen;
            return nullptr;
        }

        Query::ptr query(new Query);
        query->qid = DNS_HEADER_QID(qbuf);
        query->timeout = 0;
        query->skip_server.resize(getServerSize());

        query->m_tcpbuf.resize(qlen + 2);
        query->m_tcpbuf[0] = (qlen >> 8) & 0xff; // net endian
        query->m_tcpbuf[1] = qlen & 0xff;
        memcpy(&query->m_tcpbuf[0] + 2, &qbuf[0], qlen);

        query->qbuf = &query->m_tcpbuf[0] + 2;
        query->qlen = qlen;

        query->try_count = 0;
        query->server_idx = 0;
        for (int i = 0; i < getServerSize(); i++) {
            query->skip_server[i] = 0;
        }
        query->using_tcp = (m_flags & ARES_FLAG_USEVC) || qlen > PACKETSZ;
        query->error_status = ARES_ECONNREFUSED;
        
        RWMutexType::WriteLock lock(m_mutex);
        m_queries.insert(std::make_pair(query->qid, query));
        lock.unlock();

        query->fiber = sylar::Fiber::GetThis(); //Must set fiber before send.
        ares_send(query);
        // 此时很有可能m_queries里已经没哟Q了
        return query;
    }

    Query::ptr AresChannel::aresQuery(const std::string &name, int dnsclass, int type) {
        std::vector<uint8_t> qbuf; //in-out
        int rd = !(m_flags & ARES_FLAG_NORECURSE);
        uint16_t messageId = m_nextId++;
        int status = ares_mkquery(name.c_str(), dnsclass, type, messageId, rd, qbuf);
        if (status != ARES_SUCCESS) {
            return nullptr;
        }
        return aresSend(qbuf);
    }

    void AresChannel::QueryTimeout(std::weak_ptr<Query> q, AresChannel::ptr channel) {
        auto t = q.lock();
        if (!t) {
            return;
        }
        SYLAR_LOG_ERROR(g_logger) << "ares: " << " dns timeout";
        sylar::Scheduler::GetThis()->schedule(t->fiber);
        RWMutexType::WriteLock lock(channel->m_mutex);
        channel->m_queries.erase(t->qid);
    }

    std::vector<IPv4Address> AresChannel::aresGethostbyname(const std::string &name) {
        std::vector<IPv4Address> ips;
        Query::ptr query = aresQuery(name);
        if (!query) {
            SYLAR_LOG_ERROR(g_logger) << "aresGethostbyname query is nullptr";
            return ips;
        }

        IOManager* iom = sylar::IOManager::GetThis();
        std::weak_ptr<Query> wquery(query);
        AresChannel::ptr curr = std::dynamic_pointer_cast<AresChannel>(shared_from_this());
        Timer::ptr timer = iom->addTimer(2000, [wquery, curr]() {
            auto t = wquery.lock();
            if (!t) {
                return;
            }
            SYLAR_LOG_ERROR(g_logger) << "ares: " << " dns timeout";
            sylar::Scheduler::GetThis()->schedule(t->fiber);
            RWMutexType::WriteLock lock(curr->m_mutex);
            curr->m_queries.erase(t->qid);
        });
        sylar::Fiber::YeildToHold();
        if (timer) {
            timer->cancel();
        }
        if (query->result.size() > 0) {
            for (auto& i : query->result) {
                IPv4Address ip(ntohl(static_cast<uint32_t>(i.s_addr)));
                ips.push_back(ip);
            }
        }
        return ips;
    }

    int AresChannel::getIdxServerFd(int idx) {
        if (idx >= getServerSize()) {
            return -1;
        }
        return m_servers[idx].begin()->second->getSocket();
    }

    void AresChannel::startReceiver(Socket::ptr sock) {
        while (!isStop()) {
            std::vector<uint8_t> buf;
            buf.resize(PACKETSZ + 1);
            int count = sock->recv(&buf[0], buf.size());
            if (count <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "AresChannel::startReceiver UDP recv faild errno: " << errno
                                          << " strerrno: " << strerror(errno);
                return;
            }
            //SYLAR_LOG_DEBUG(g_logger) << "AresChannel::starReceiver ret: " << count;
            size_t which_server = 0;
            for (; which_server < m_servers.size(); which_server++) {
                if (getIdxServerFd(which_server) == sock->getSocket()) {
                    break;
                }
            }
            processAnswer(&buf[0], count, which_server, 0);
        }
        return;
    }

    int AresChannel::aresParseReply(uint8_t* abuf, int alen, std::vector<in_addr>& result) {
        int status, i, rr_type, rr_class, rr_len, naddrs;
        int naliases;
        int len;
        uint8_t *aptr;
        std::vector<uint8_t> rr_data;
        std::vector<uint8_t> rr_name;
        std::vector<uint8_t> hostname;
        std::vector<in_addr> addrs;
        std::vector<std::string> aliases;

        if (alen < HFIXEDSZ) {
            SYLAR_LOG_ERROR(g_logger) << "aresParseReply alen too short: " << alen;
            return ARES_EBADRESP;
        }

        uint32_t qdcount = DNS_HEADER_QDCOUNT(abuf);
        uint32_t ancount = DNS_HEADER_ANCOUNT(abuf);
        if (qdcount != 1) {
            SYLAR_LOG_ERROR(g_logger) << "aresParseReply qdcount " << qdcount;
            return ARES_EBADRESP;
        }

        aptr = abuf + HFIXEDSZ;
        status = aresExpandName(aptr, abuf, alen, hostname, &len);
        if (status != ARES_SUCCESS) return status;
        if (aptr + len + QFIXEDSZ > abuf + alen) {
            return ARES_EBADRESP;
        }
        aptr += len + QFIXEDSZ; //In ans

        /* Allocate addresses and aliases; ancount gives an upper bound for both. */
        addrs.resize(ancount);
        aliases.resize(ancount + 1);
        naddrs = 0;
        naliases = 0;

        /* Examine each answer resource record (RR) in turn. */
        for (i = 0; i < (int) ancount; i++) {
            /* Decode the RR up to the data field. */
            status = aresExpandName(aptr, abuf, alen, rr_name, &len);
            if (status != ARES_SUCCESS) break;
            aptr += len;
            if (aptr + RRFIXEDSZ > abuf + alen) {
                status = ARES_EBADRESP;
                break;
            }
            rr_type = DNS_RR_TYPE(aptr);
            rr_class = DNS_RR_CLASS(aptr);
            rr_len = DNS_RR_LEN(aptr);
            aptr += RRFIXEDSZ;

            if (rr_class == C_IN && rr_type == T_A && rr_len == sizeof(struct in_addr)
                && strcasecmp((const char *) &rr_name[0], (const char *) &hostname[0]) == 0) {
                memcpy(&addrs[naddrs], aptr, sizeof(struct in_addr));
                naddrs++;
                status = ARES_SUCCESS;
            } //正是查询域名的a记录 则拷到a记录数组中

            if (rr_class == C_IN && rr_type == T_CNAME) { //cname 则把具体的域名 放到alias数组中
                /* Record the RR name as an alias. */
                aliases.push_back(std::string((char *) &rr_name[0], rr_name.size()));
                naliases++;
                /* Decode the RR data and replace the hostname with it. */
                status = aresExpandName(aptr, abuf, alen, rr_data, &len);
                if (status != ARES_SUCCESS) break;
                hostname = rr_data;
            } else {
                rr_name.clear();
            }

            aptr += rr_len;
            if (aptr > abuf + alen) {
                status = ARES_EBADRESP;
                break;
            }
        }

        if (status == ARES_SUCCESS && naddrs == 0) status = ARES_ENODATA;
        if (status == ARES_SUCCESS) { //只要有a记录 就当做是成功
            /* We got our answer.  Allocate memory to build the host entry. */
            {
                {
                    addrs.resize(naddrs);
                    result = addrs;
                    return ARES_SUCCESS;
                }
            }
            status = ARES_ENOMEM;
        }
        return status;
    }

    void AresChannel::processAnswer(uint8_t* abuf, int alen, int whichserver, int tcp) {
        if (alen < HFIXEDSZ) {
            SYLAR_LOG_ERROR(g_logger) << "processAnswer alen to short: " << alen;
            return;
        }

        int id = DNS_HEADER_QID(abuf);
        int tc = DNS_HEADER_TC(abuf);
        int rcode = DNS_HEADER_RCODE(abuf);

        auto it = m_queries.find(id);
        if (it == m_queries.end()) {
            SYLAR_LOG_ERROR(g_logger) << "m_queries not has this id: "
                                      << id;
            return;
        }
        Query::ptr query = it->second;

        if ((tc || alen > PACKETSZ) && !tcp && !(m_flags & ARES_FLAG_IGNTC)) { //如果包里明确有trunc 或 包长大于512字节
            if (!query->using_tcp) {
                query->using_tcp = 1;
                ares_send(query);  //在收到trunc响应包后 立即用tcp请求 跟pdns一致
            }
            return;
        }

        if (alen > PACKETSZ && !tcp) {
            alen = PACKETSZ;
        }

        if (!(m_flags & ARES_FLAG_NOCHECKRESP)) {
            if (rcode == SERVFAIL || rcode == NOTIMP || rcode == REFUSED) {
                query->skip_server[whichserver] = 1;
                if (query->server_idx == whichserver) {
                    nextServer(query);
                }
                return;
            }
            /*
            if (!same_questions(query->qbuf, query->qlen, abuf, alen)) {
                if (query->server == whichserver)
                    next_server(query, now);
                return;
            }
             */
        }

        aresParseReply(abuf, alen, query->result);
        sylar::Scheduler::GetThis()->schedule(query->fiber);
        //sylar::Scheduler::GetThis()->schedule(&query->fiber);

        RWMutexType::WriteLock lock(m_mutex);
        m_queries.erase(query->qid);
    }
}

//设计悖论:
//1. channel中用map<shared_ptr> 那么只有当从map把q摘下来时 q才有可能引用为0  如果是这种容器+shared_ptr无论你怎么自定义release 都不会调这个release
//	所以要想用自定义的删除器 容器里存放的必然是裸指针
