#include "ns/ares.hh"
#include <string.h>
#include <stdlib.h>
#include "util.hh"
#include "log.hh"
#include "iomanager.hh"

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    static char* try_config(char* s, const char* opt) {
        size_t len;
        len = strlen(opt);
        if (strncmp(s, opt, len) != 0 || !isspace((unsigned char)s[len]))
            return NULL;
        s += len;
        while (isspace((unsigned char)*s))
            s++;
        return s;
    }

    static const char* try_option(const char* p, const char* q, const char* opt) {
        size_t len = strlen(opt);
        return ((size_t)(q - p) > len && !strncmp(p, opt, len)) ? &p[len] : NULL;
    }

    static int set_search(AresChannel::ptr channel, const char* str) {
        int n;
        const char* p, *q;

        /* Count the domains given. */
        n = 0;
        p = str;
        while (*p) {
            while (*p && !isspace((unsigned char)*p))
                p++;
            while (isspace((unsigned char)*p))
                p++;
            n++;
        }

        channel->domains = malloc(n * sizeof(char *));
        if (!channel->domains && n)
            return ARES_ENOMEM;

        /* Now copy the domains. */
        n = 0;
        p = str;
        while (*p) {
            channel->ndomains = n;
            q = p;
            while (*q && !isspace((unsigned char)*q))
                q++;
            channel->domains[n] = malloc(q - p + 1);
            if (!channel->domains[n])
                return ARES_ENOMEM;
            memcpy(channel->domains[n], p, q - p);
            channel->domains[n][q - p] = 0;
            p = q;
            while (isspace((unsigned char)*p))
                p++;
            n++;
        }
        channel->ndomains = n;
        return ARES_SUCCESS;
    }

    static int config_nameserver(std::vector<ServerState::ptr>& servers, char* str) {
        ServerState::ptr newserv(new ServerState);
        struct in_addr addr;
        addr.s_addr = inet_addr(str); //Add a nameserver entry, if this is a valid address
        if (addr.s_addr == INADDR_NONE)
            return ARES_SUCCESS;
        newserv->addr = addr;
        servers.push_back(newserv);
        return ARES_SUCCESS;
    }

    static int init_by_resolv_conf(AresChannel* channel) {
        int status = 0;
        std::vector<ServerState> servers;
        {
            char* p;
            std::string line;
            std::ifstream ifs;
            if (!sylar::FSUtil::OpenForRead(ifs, PATH_RESOLV_CONF, std::ios::binary)) {
                SYLAR_LOG_ERROR(g_logger) << "Can not open " << PATH_RESOLV_CONF << " errno: "
                << errno << " strerrno: " << strerror(errno);
                return 0;
            }
            while (getline(ifs, line)) {
                if ((p = try_config(line.c_str(), "domain")) )
                    status = set_search(channel, p);
                else if ((p = try_config(line.c_str(), "nameserver")) )
                    status = config_nameserver(servers, p);
                else
                    status = ARES_SUCCESS;
                if (status != ARES_SUCCESS)
                    break;
            }
        }

        if(status != ARES_EOF) {
            return status;
        }

        if(servers.size()) {
            channel->m_servers = std::move(servers);
        }
    }
    ///////////////////////////////////////////////////////////////////////////

    int ServerState::openTcpSocket(uint16_t port) {
        int s, flags;
        struct sockaddr_in sockin;
        s = socket(AF_INET, SOCK_STREAM, 0);
        if(s == -1)
            return -1;
        flags = fcntl(s, F_GETFL, 0);
        if (flags == -1) {
            close(s);
            return -1;
        }
        flags |= O_NONBLOCK;
        if(fcntl(s, F_SETFL, flags) == -1) {
            close(s);
            return -1;
        }
        /* Connect to the server. */
        memset(&sockin, 0, sizeof(sockin));
        sockin.sin_family = AF_INET;
        sockin.sin_addr = addr;
        sockin.sin_port = port;
        if(connect(s, (struct sockaddr *)&sockin, sizeof(sockin)) == -1 && errno != EINPROGRESS) {
            ares_closesocket(s);
            return -1;
        }
        m_tcp_socket = s;
        return 0;
    }

    int ServerState::openUdpSocket(uint16_t port) {
        int s;
        struct sockaddr_in sockin;
        /* Acquire a socket. */
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if(s == -1) return -1;
        /* Connect to the server. */
        memset(&sockin, 0, sizeof(sockin));
        sockin.sin_family = AF_INET;
        sockin.sin_addr = addr;
        sockin.sin_port = udp_port;
        if(connect(s, (struct sockaddr *) &sockin, sizeof(sockin)) == -1) {
            ares_closesocket(s);
            return -1;
        }
        m_udp_socket = s;
        return 0;
    }

    int AresChannel::aresRegistFds() {
        for(const auto& i : m_servers) {
            if(i->udp_socket != -1) {
                sylar::IOManager::GetThis()->addEvent(i->udp_socket, IOManager::READ,
                        std::bind(&AresChannel::AresUDPCallBack, this));
            }
            if(i->tcp_socket != -1) {
                sylar::IOManager::GetThis()->addEvent(i->tcp_socket, IOManager::READ,
                        std::bind(&AresChannel::AresTCPCallBack, this));
            }
        }
        return 0;
    }

    void AresChannel::AresUDPCallBack(int fd) {
        std::vector<uint8_t> buf;
        buf.resize(PACKETSZ + 1);
        int which_server = -1;

        int count = recv(fd, &buf[0], buf.size(), 0);
        if (count <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "UDP recv faild errno: " << errno
            << " strerrno: " << strerror(errno);
            return;
        }
        for (size_t i = 0; i < m_servers.size(); i++) {
            if (m_servers[i]->getTCPFd() == fd ||
                m_servers[i]->getUDPFd() == fd) {
                which_server = i;
                break;
            }
        }
        processAnswer(&buf[0], count, which_server, 0);
    }

    AresChannel::AresChannel()
    : AresOptions() {
    }

    void AresChannel::init() {
        struct timeval tv;
        init_by_resolv_conf(this);
        gettimeofday(&tv, NULL);
        next_id = (unsigned short) (tv.tv_sec ^ tv.tv_usec ^ getpid()) & 0xffff;
        queries = NULL;
    }

    void AresChannel::AresCallBack() {

    }

    void AresChannnel::nextServer(Query::ptr query) {
        query->server_idx++;
        for(; query->try_count < channel->m_tries; query->try_count++) { // try小于全局定义的次数
            for(; query->server_idx < getServerSize(); query->server_idx++) {
                if(!query->skip_server[query->server_idx]) {
                    aresSend(query);
                    return;
                }
            }
            query->server_idx = 0;

            if(query->using_tcp)
                break;
        }
        //end_query(channel, query, query->error_status, NULL, 0);
    }

    void AresChannel::ares_send(Query::ptr query, time_t now) {
        struct ServerState* server;

        server = &servers[query->server_idx];
        if(query->using_tcp) {
            if (server->tcp_socket == -1) {
                if (openTcpSocket(m_tcp_port) == -1) {
                    query->skip_server[query->server_idx] = 1;
                    nextServer(query, now);
                    return;
                }
            }
            SendRequest::ptr sendreq(new SendRequest);
            sendreq->data = &query->tcpbuf[0];
            sendreq->len = query->tcplen;
            query->timeout = 0;
        }
        else {
            if(server->udp_socket == -1) {
                if (openUdpSocket(m_udp_port) == -1) {
                    query->skip_server[query->server_idx] = 1;
                    nextServer(query, now);
                    return;
                }
            }
            if(send(server->udp_socket, query->qbuf, query->qlen, 0) == -1) {
                query->skip_server[query->server_idx] = 1;
                nextServer(query, now);
                return;
            }
            query->timeout = now + ((query->try == 0) ? m_timeout : m_timeout << query->try_count / getServer().size());
        }
    }

    int AresChannel::ares_mkquery(const char* name, int dnsclass, int type, unsigned short id, int rd,
            std::vector<unsigned char>& buf) {
        int len;
        unsigned char *q;
        const char *p;

        /* Compute the length of the encoded name so we can check buflen.
         * Start counting at 1 for the zero-length label at the end. */
        len = 1;
        for(p = name; *p; p++) {
            if (*p == '\\' && *(p + 1) != 0) p++;
            len++;
        }
        /* If there are n periods in the name, there are n + 1 labels, and
         * thus n + 1 length fields, unless the name is empty or ends with a
         * period.  So add 1 unless name is empty or ends with a period.
         */
        if(*name && *(p - 1) != '.')  //顶级域必须有点结尾
            len++;

        buf.resize(len + HFIXEDSZ + QFIXEDSZ)

        /* Set up the header. */
        q = &buf[0];
        memset(q, 0, HFIXEDSZ);
        DNS_HEADER_SET_QID(q, id);
        DNS_HEADER_SET_OPCODE(q, QUERY);
        DNS_HEADER_SET_RD(q, (rd) ? 1 : 0);
        DNS_HEADER_SET_QDCOUNT(q, 1);

        /* A name of "." is a screw case for the loop below, so adjust it. */
        if(strcmp(name, ".") == 0)
            name++;

        /* Start writing out the name after the header. */
        q += HFIXEDSZ;
        while(*name) {
            if(*name == '.') return ARES_EBADNAME;

            /* Count the number of bytes in this label. */
            len = 0;
            for(p = name; *p && *p != '.'; p++) {
                if(*p == '\\' && *(p + 1) != 0)
                    p++;
                len++;
            }
            if(len > MAXLABEL) return ARES_EBADNAME;

            /* Encode the length and copy the data. */
            *q++ = len;
            for(p = name; *p && *p != '.'; p++) {
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

    static int name_length(const uint8_t* encoded, const uint_8* abuf, int alen) { //返回压缩前 域名长度 包括点
        int n = 0, offset, indir = 0;

        if(encoded == abuf + alen) return -1;

        while(*encoded) {
            if((*encoded & INDIR_MASK) == INDIR_MASK) { //0xc0
                /* Check the offset and go there. */
                if(encoded + 1 >= abuf + alen) return -1;
                offset = (*encoded & ~INDIR_MASK) << 8 | *(encoded + 1);
                if(offset >= alen) return -1;
                encoded = abuf + offset;

                /* If we've seen more indirects than the message length,
                 * then there's a loop.
                 */
                if(++indir > alen) return -1;
            } else {
                offset = *encoded;
                if(encoded + offset + 1 >= abuf + alen) return -1;
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

    int AresChannel::aresExpandName(uint8_t* encoded, const uint8_t* abuf, int alen,
            std::vector<uint8_t>& s, int* enclen) {
        int len, indir = 0;
        const uint8_t* p;

        len = name_length(encoded, abuf, alen); //仅是assert作用 感觉很多余?
        if(len == -1) return ARES_EBADNAME;

        s.resize(len + 1);
        uint8_t* q = &s[0];

        /* No error-checking necessary; it was all done by name_length(). */
        p = encoded;
        while(*p) { //拷贝
            if((*p & INDIR_MASK) == INDIR_MASK) {
                if(!indir) {
                    *enclen = p + 2 - encoded;
                    indir = 1;
                }
                p = abuf + ((*p & ~INDIR_MASK) << 8 | *(p + 1));
            } else {
                len = *p;
                p++;
                while(len--) {
                    if(*p == '.' || *p == '\\')
                        *q++ = '\\';
                    *q++ = *p;
                    p++;
                }
                *q++ = '.';
            }
        }
        if(!indir)
            *enclen = p + 1 - encoded;

        /* Nuke the trailing period if we wrote one. */
        if (q > *s)
            *(q - 1) = 0;

        return ARES_SUCCESS;
    }

    void AresChannel::aresSend(std::vector<unsigned char>& qbuf) {
        int qlen = qbuf.size();
        if(qlen < HFIXEDSZ || qlen >= (1 << 16)) {
            SYLAR_LOG_ERROR(g_logger) << "qlen too small or too max " << qlen;
            return;
        }

        Query::ptr query(new Query);
        query->qid = DNS_HEADER_QID(qbuf);
        query->timeout = 0;
        query->skip_server.resize(getServer().size());

        query->m_tcpbuf.resize(qlen + 2);
        query->tcpbuf[0] = (qlen >> 8) & 0xff; // net endian
        query->tcpbuf[1] = qlen & 0xff;
        memcpy(&query->tcpbuf[0] + 2, qbuf, qlen);

        query->qbuf = &query->tcpbuf[0] + 2;
        query->qlen = qlen;
        query->callback = callback;

        query->try_count = 0;
        query->server_idx = 0;
        for (size_t i = 0; i < getServer().size(); i++)
            query->skip_server[i] = 0;
        query->using_tcp = (flags & ARES_FLAG_USEVC) || qlen > PACKETSZ;
        query->error_status = ARES_ECONNREFUSED;

        m_queries.push_back(query);

        time_t now;
        time(&now);
        ares_send(query, now);
    }

    void AresChannel::aresQuery(const char* name, int dnsclass, int type) {
        std::vector<unsigned char> qbuf;
        int rd, status;

        /* Compose the query. */
        rd = !(flags & ARES_FLAG_NORECURSE);
        status = ares_mkquery(name, dnsclass, type, next_id, rd, qbuf);
        channel->next_id++;
        if(status != ARES_SUCCESS) {
            return;
        }

        aresSend(qbuf);
    }

    void AresChannel::aresGethostbyname(const char* name, int family) {
        aresQuery(name, C_IN, T_A);
    }

    int AresChannel::aresParseResponse(uint8_t* abuf, int alen, struct hostent** host) {
        int status, i, rr_type, rr_class, rr_len, naddrs;
        int naliases;
        long len;
        const unsigned char *aptr;
        char *hostname, *rr_name, *rr_data, **aliases;
        struct in_addr *addrs;
        struct hostent *hostent;
        *host = NULL;

        if(alen < HFIXEDSZ) {
            SYLAR_LOG_ERROR(g_logger) << "aresParseResponse alen too short: " << alen;
            return ARES_EBADRESP;
        }

        uint32_t qdcount = DNS_HEADER_QDCOUNT(abuf);
        uint32_t ancount = DNS_HEADER_ANCOUNT(abuf);
        if(qdcount != 1) {
            SYLAR_LOG_ERROR(g_logger) << "aresParseResponse qdcount " << qdcount;
            return ARES_EBADRESP;
        }

        aptr = abuf + HFIXEDSZ;
        status = aresExpandName(aptr, abuf, alen, &hostname, &len);
        if(status != ARES_SUCCESS) return status;
        if(aptr + len + QFIXEDSZ > abuf + alen) {
            free(hostname);
            return ARES_EBADRESP;
        }
        aptr += len + QFIXEDSZ; //In ans

        /* Allocate addresses and aliases; ancount gives an upper bound for both. */
        addrs = malloc(ancount * sizeof(struct in_addr));
        if (!addrs) {
            free(hostname);
            return ARES_ENOMEM;
        }
        aliases = malloc((ancount + 1) * sizeof(char *));
        if(!aliases) {
            free(hostname);
            free(addrs);
            return ARES_ENOMEM;
        }
        naddrs = 0;
        naliases = 0;

        /* Examine each answer resource record (RR) in turn. */
        for(i = 0; i < (int)ancount; i++) {
            /* Decode the RR up to the data field. */
            status = aresExpandName(aptr, abuf, alen, &rr_name, &len);
            if(status != ARES_SUCCESS) break;
            aptr += len;
            if(aptr + RRFIXEDSZ > abuf + alen) {
                status = ARES_EBADRESP;
                break;
            }
            rr_type = DNS_RR_TYPE(aptr);
            rr_class = DNS_RR_CLASS(aptr);
            rr_len = DNS_RR_LEN(aptr);
            aptr += RRFIXEDSZ;

            if(rr_class == C_IN && rr_type == T_A && rr_len == sizeof(struct in_addr) && strcasecmp(rr_name, hostname) == 0) {
                memcpy(&addrs[naddrs], aptr, sizeof(struct in_addr));
                naddrs++;
                status = ARES_SUCCESS;
            } //正是查询域名的a记录 则拷到a记录数组中

            if(rr_class == C_IN && rr_type == T_CNAME) { //cname 则把具体的域名 放到alias数组中
                /* Record the RR name as an alias. */
                aliases[naliases] = rr_name;
                naliases++
                /* Decode the RR data and replace the hostname with it. */
                status = aresExpandName(aptr, abuf, alen, &rr_data, &len);
                if(status != ARES_SUCCESS) break;
                free(hostname);
                hostname = rr_data;
            } else
                free(rr_name);

            aptr += rr_len;
            if(aptr > abuf + alen) {
                status = ARES_EBADRESP;
                break;
            }
        }

        if(status == ARES_SUCCESS && naddrs == 0) status = ARES_ENODATA;
        if(status == ARES_SUCCESS) { //只要有a记录 就当做是成功
            /* We got our answer.  Allocate memory to build the host entry. */
            aliases[naliases] = NULL;
            hostent = malloc(sizeof(struct hostent));
            if(hostent) {
                hostent->h_addr_list = malloc((naddrs + 1) * sizeof(char *));
                if(hostent->h_addr_list) {
                    /* Fill in the hostent and return successfully. */
                    hostent->h_name = hostname;
                    hostent->h_aliases = aliases;
                    hostent->h_addrtype = AF_INET;
                    hostent->h_length = sizeof(struct in_addr);
                    for (i = 0; i < naddrs; i++)
                        hostent->h_addr_list[i] = (char *) &addrs[i];
                    hostent->h_addr_list[naddrs] = NULL;
                    *host = hostent;
                    return ARES_SUCCESS;
                }
                free(hostent);
            }
            status = ARES_ENOMEM;
        }
        for(i = 0; i < naliases; i++)
            free(aliases[i]);
        free(aliases);
        free(addrs);
        free(hostname);
        return status;
    }

    int AresChannel::aresParsePtrReply(const unsigned char *abuf, int alen, const void *addr, int addrlen, int family, struct hostent **host) {
        unsigned int qdcount, ancount;
        int status, i, rr_type, rr_class, rr_len;
        long len;
        const unsigned char *aptr;
        char *ptrname, *hostname, *rr_name, *rr_data;
        struct hostent *hostent;

        /* Set *host to NULL for all failure cases. */
        *host = NULL;

        /* Give up if abuf doesn't have room for a header. */
        if (alen < HFIXEDSZ)
            return ARES_EBADRESP;

        /* Fetch the question and answer count from the header. */
        qdcount = DNS_HEADER_QDCOUNT(abuf);
        ancount = DNS_HEADER_ANCOUNT(abuf);
        if (qdcount != 1)
            return ARES_EBADRESP;

        /* Expand the name from the question, and skip past the question. */
        aptr = abuf + HFIXEDSZ;
        status = ares_expand_name(aptr, abuf, alen, &ptrname, &len);
        if (status != ARES_SUCCESS)
            return status;
        if (aptr + len + QFIXEDSZ > abuf + alen)
        {
            free(ptrname);
            return ARES_EBADRESP;
        }
        aptr += len + QFIXEDSZ;

        /* Examine each answer resource record (RR) in turn. */
        hostname = NULL;
        for (i = 0; i < (int)ancount; i++)
        {
            /* Decode the RR up to the data field. */
            status = ares_expand_name(aptr, abuf, alen, &rr_name, &len);
            if (status != ARES_SUCCESS)
                break;
            aptr += len;
            if (aptr + RRFIXEDSZ > abuf + alen)
            {
                status = ARES_EBADRESP;
                break;
            }
            rr_type = DNS_RR_TYPE(aptr);
            rr_class = DNS_RR_CLASS(aptr);
            rr_len = DNS_RR_LEN(aptr);
            aptr += RRFIXEDSZ;

            if (rr_class == C_IN && rr_type == T_PTR
                && strcasecmp(rr_name, ptrname) == 0)
            {
                /* Decode the RR data and set hostname to it. */
                status = ares_expand_name(aptr, abuf, alen, &rr_data, &len);
                if (status != ARES_SUCCESS)
                    break;
                if (hostname)
                    free(hostname);
                hostname = rr_data;
            }

            if (rr_class == C_IN && rr_type == T_CNAME)
            {
                /* Decode the RR data and replace ptrname with it. */
                status = ares_expand_name(aptr, abuf, alen, &rr_data, &len);
                if (status != ARES_SUCCESS)
                    break;
                free(ptrname);
                ptrname = rr_data;
            }

            free(rr_name);
            aptr += rr_len;
            if (aptr > abuf + alen)
            {
                status = ARES_EBADRESP;
                break;
            }
        }

        if (status == ARES_SUCCESS && !hostname)
            status = ARES_ENODATA;
        if (status == ARES_SUCCESS)
        {
            /* We got our answer.  Allocate memory to build the host entry. */
            hostent = malloc(sizeof(struct hostent));
            if (hostent)
            {
                hostent->h_addr_list = malloc(2 * sizeof(char *));
                if (hostent->h_addr_list)
                {
                    hostent->h_addr_list[0] = malloc(addrlen);
                    if (hostent->h_addr_list[0])
                    {
                        hostent->h_aliases = malloc(sizeof (char *));
                        if (hostent->h_aliases)
                        {
                            /* Fill in the hostent and return successfully. */
                            hostent->h_name = hostname;
                            hostent->h_aliases[0] = NULL;
                            hostent->h_addrtype = family;
                            hostent->h_length = addrlen;
                            memcpy(hostent->h_addr_list[0], addr, addrlen);
                            hostent->h_addr_list[1] = NULL;
                            *host = hostent;
                            free(ptrname);
                            return ARES_SUCCESS;
                        }
                        free(hostent->h_addr_list[0]);
                    }
                    free(hostent->h_addr_list);
                }
                free(hostent);
            }
            status = ARES_ENOMEM;
        }
        if (hostname)
            free(hostname);
        free(ptrname);
        return status;
    }

    void AresChannel::readTCPData(time_t now) {
        int count;
        for (size_t i = 0; i < getServer().size(); i++) {
            ServerState::ptr server = m_servers[i];
            if (server->tcp_socket == -1)
                continue;

            count = recv(server->tcp_socket,
                         &server->tcp_buffer[0],
                         server->tcp_buffer.size(), 0);
            if (count <= 0) {
                continue;
            }

            server->tcp_buffer_pos += count;
            if (server->tcp_buffer_pos == server->tcp_length) {
                processAnswer(&server->tcp_buffer[0], server->tcp_length,
                               i, 1, now);
                server->tcp_lenbuf_pos = 0;
            }
        }
    }

    void AresChannel::endQuery(Query::ptr query, int status, unsigned char* abuf, int alen) {
        int i;

        //query->callback(query->arg, status, abuf, alen);
        for (const auto &i : m_queries) {
            if (i == query) break;
        }
        *q = query->next;
        free(query->tcpbuf);
        free(query->skip_server);
        free(query);

        /* Simple cleanup policy: if no queries are remaining, close all
         * network sockets unless STAYOPEN is set.
         */
        if (!m_queries && !(flags & ARES_FLAG_STAYOPEN)) {
            for (i = 0; i < getServer().size(); i++) ares__close_sockets(&channel->servers[i]);
        }
    }

    void AresChannel::processAnswer(uint8_t* abuf, int alen, int whichserver, int tcp) {
        if (alen < HFIXEDSZ) {
            SYLAR_LOG_ERROR(g_logger) << "processAnswer alen to short: " << alen;
            return;
        }

        int id = DNS_HEADER_QID(abuf);
        int tc = DNS_HEADER_TC(abuf);
        int rcode = DNS_HEADER_RCODE(abuf);

        auto it = m_queries.find(id)
        if (it == m_queries.end()) {
            SYLAR_LOG_ERROR(g_logger) << "m_queries not has this id: "
            << id;
            return;
        }
        Query::ptr query = it->second;

        if ((tc || alen > PACKETSZ) && !tcp && !(flags & ARES_FLAG_IGNTC)) { //如果包里明确有trunc 或 包长大于512字节
            if (!query->using_tcp) {
                query->using_tcp = 1;
                aresSend(query);  //在收到trunc响应包后 立即用tcp请求 跟pdns一致
            }
            return;
        }

        if (alen > PACKETSZ && !tcp) {
            alen = PACKETSZ;
        }

        if (!(flags & ARES_FLAG_NOCHECKRESP)) {
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

        end_query(query, ARES_SUCCESS, abuf, alen);
    }

/* If any TCP sockets select true for writing, write out queued data
 * we have for them.
 */
    void AresChannel::writeTcpData(fd_set *write_fds, time_t now) {
        struct server_state *server;
        struct send_request *sendreq;
        struct iovec *vec;
        int i, n, count;

        for (i = 0; i < channel->nservers; i++)
        {
            /* Make sure server has data to send and is selected in write_fds. */
            server = &channel->servers[i];
            if (!server->qhead || server->tcp_socket == -1
                || !FD_ISSET(server->tcp_socket, write_fds))
                continue;

            /* Count the number of send queue items. */
            n = 0;
            for (sendreq = server->qhead; sendreq; sendreq = sendreq->next)
                n++;

            /* Allocate iovecs so we can send all our data at once. */
            vec = malloc(n * sizeof(struct iovec));
            if (vec)
            {
                /* Fill in the iovecs and send. */
                n = 0;
                for (sendreq = server->qhead; sendreq; sendreq = sendreq->next)
                {
                    vec[n].iov_base = (char *) sendreq->data;
                    vec[n].iov_len = sendreq->len;
                    n++;
                }
                count = writev(server->tcp_socket, vec, n);
                free(vec);
                if (count < 0)
                {
                    handle_error(channel, i, now);
                    continue;
                }

                /* Advance the send queue by as many bytes as we sent. */
                while (count)
                {
                    sendreq = server->qhead;
                    if ((size_t)count >= sendreq->len)
                    {
                        count -= sendreq->len;
                        server->qhead = sendreq->next;
                        if (server->qhead == NULL)
                            server->qtail = NULL;
                        free(sendreq);
                    }
                    else
                    {
                        sendreq->data += count;
                        sendreq->len -= count;
                        break;
                    }
                }
            }
            else
            {
                /* Can't allocate iovecs; just send the first request. */
                sendreq = server->qhead;

                count = send(server->tcp_socket, sendreq->data, sendreq->len, 0);

                if (count < 0)
                {
                    handle_error(channel, i, now);
                    continue;
                }

                /* Advance the send queue by as many bytes as we sent. */
                if ((size_t)count == sendreq->len)
                {
                    server->qhead = sendreq->next;
                    if (server->qhead == NULL)
                        server->qtail = NULL;
                    free(sendreq);
                }
                else
                {
                    sendreq->data += count;
                    sendreq->len -= count;
                }
            }
        }
    }


}