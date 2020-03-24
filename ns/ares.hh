#ifndef __ARES_HH__
#define __ARES_HH__

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <sys/types.h>
#include <netinet/in.h>
#include <unordered_map>
#include "thread.hh"

#define ARES_SUCCESS		0

/* Server error codes (ARES_ENODATA indicates no relevant answer) */
#define ARES_ENODATA		1
#define ARES_EFORMERR		2
#define ARES_ESERVFAIL		3
#define ARES_ENOTFOUND		4
#define ARES_ENOTIMP		5
#define ARES_EREFUSED		6

/* Locally generated error codes */
#define ARES_EBADQUERY		7
#define ARES_EBADNAME		8
#define ARES_EBADFAMILY		9
#define ARES_EBADRESP		10
#define ARES_ECONNREFUSED	11
#define ARES_ETIMEOUT		12
#define ARES_EOF		13
#define ARES_EFILE		14
#define ARES_ENOMEM		15
#define ARES_EDESTRUCTION	16

/* Flag values */
#define ARES_FLAG_USEVC		(1 << 0) //TCP?
#define ARES_FLAG_PRIMARY	(1 << 1) //Only use one dns server
#define ARES_FLAG_IGNTC		(1 << 2)
#define ARES_FLAG_NORECURSE	(1 << 3)
#define ARES_FLAG_STAYOPEN	(1 << 4)
#define ARES_FLAG_NOSEARCH	(1 << 5)
#define ARES_FLAG_NOALIASES	(1 << 6)
#define ARES_FLAG_NOCHECKRESP	(1 << 7)

/* Option mask values */
#define ARES_OPT_FLAGS		(1 << 0)
#define ARES_OPT_TIMEOUT	(1 << 1)
#define ARES_OPT_TRIES		(1 << 2)
#define ARES_OPT_NDOTS		(1 << 3)
#define ARES_OPT_UDP_PORT	(1 << 4)
#define ARES_OPT_TCP_PORT	(1 << 5)
#define ARES_OPT_SERVERS	(1 << 6)
#define ARES_OPT_DOMAINS	(1 << 7)
#define ARES_OPT_LOOKUPS	(1 << 8)

#define DNS__16BIT(p)			(((p)[0] << 8) | (p)[1])
#define DNS__32BIT(p)			(((p)[0] << 24) | ((p)[1] << 16) | \
					 ((p)[2] << 8) | (p)[3])
#define DNS__SET16BIT(p, v)		(((p)[0] = ((v) >> 8) & 0xff), \
					 ((p)[1] = (v) & 0xff))
#define DNS__SET32BIT(p, v)		(((p)[0] = ((v) >> 24) & 0xff), \
					 ((p)[1] = ((v) >> 16) & 0xff), \
					 ((p)[2] = ((v) >> 8) & 0xff), \
					 ((p)[3] = (v) & 0xff))

/* Macros for parsing a DNS header */
#define DNS_HEADER_QID(h)		DNS__16BIT(h)
#define DNS_HEADER_QR(h)		(((h)[2] >> 7) & 0x1)
#define DNS_HEADER_OPCODE(h)		(((h)[2] >> 3) & 0xf)
#define DNS_HEADER_AA(h)		(((h)[2] >> 2) & 0x1)
#define DNS_HEADER_TC(h)		(((h)[2] >> 1) & 0x1)
#define DNS_HEADER_RD(h)		((h)[2] & 0x1)
#define DNS_HEADER_RA(h)		(((h)[3] >> 7) & 0x1)
#define DNS_HEADER_Z(h)			(((h)[3] >> 4) & 0x7)
#define DNS_HEADER_RCODE(h)		((h)[3] & 0xf)
#define DNS_HEADER_QDCOUNT(h)		DNS__16BIT((h) + 4)
#define DNS_HEADER_ANCOUNT(h)		DNS__16BIT((h) + 6)
#define DNS_HEADER_NSCOUNT(h)		DNS__16BIT((h) + 8)
#define DNS_HEADER_ARCOUNT(h)		DNS__16BIT((h) + 10)

/* Macros for constructing a DNS header */
#define DNS_HEADER_SET_QID(h, v)	DNS__SET16BIT(h, v)
#define DNS_HEADER_SET_QR(h, v)		((h)[2] |= (((v) & 0x1) << 7))
#define DNS_HEADER_SET_OPCODE(h, v)	((h)[2] |= (((v) & 0xf) << 3))
#define DNS_HEADER_SET_AA(h, v)		((h)[2] |= (((v) & 0x1) << 2))
#define DNS_HEADER_SET_TC(h, v)		((h)[2] |= (((v) & 0x1) << 1))
#define DNS_HEADER_SET_RD(h, v)		((h)[2] |= (((v) & 0x1)))
#define DNS_HEADER_SET_RA(h, v)		((h)[3] |= (((v) & 0x1) << 7))
#define DNS_HEADER_SET_Z(h, v)		((h)[3] |= (((v) & 0x7) << 4))
#define DNS_HEADER_SET_RCODE(h, v)	((h)[3] |= (((v) & 0xf)))
#define DNS_HEADER_SET_QDCOUNT(h, v)	DNS__SET16BIT((h) + 4, v)
#define DNS_HEADER_SET_ANCOUNT(h, v)	DNS__SET16BIT((h) + 6, v)
#define DNS_HEADER_SET_NSCOUNT(h, v)	DNS__SET16BIT((h) + 8, v)
#define DNS_HEADER_SET_ARCOUNT(h, v)	DNS__SET16BIT((h) + 10, v)

/* Macros for parsing the fixed part of a DNS question */
#define DNS_QUESTION_TYPE(q)		DNS__16BIT(q)
#define DNS_QUESTION_CLASS(q)		DNS__16BIT((q) + 2)

/* Macros for constructing the fixed part of a DNS question */
#define DNS_QUESTION_SET_TYPE(q, v)	DNS__SET16BIT(q, v)
#define DNS_QUESTION_SET_CLASS(q, v)	DNS__SET16BIT((q) + 2, v)

/* Macros for parsing the fixed part of a DNS resource record */
#define DNS_RR_TYPE(r)			DNS__16BIT(r)
#define DNS_RR_CLASS(r)			DNS__16BIT((r) + 2)
#define DNS_RR_TTL(r)			DNS__32BIT((r) + 4)
#define DNS_RR_LEN(r)			DNS__16BIT((r) + 8)

/* Macros for constructing the fixed part of a DNS resource record */
#define DNS_RR_SET_TYPE(r)		DNS__SET16BIT(r, v)
#define DNS_RR_SET_CLASS(r)		DNS__SET16BIT((r) + 2, v)
#define DNS_RR_SET_TTL(r)		DNS__SET32BIT((r) + 4, v)
#define DNS_RR_SET_LEN(r)		DNS__SET16BIT((r) + 8, v)

#define ares_closesocket(x) close(x)

#define	DEFAULT_TIMEOUT		5
#define DEFAULT_TRIES		4
#ifndef INADDR_NONE
#define	INADDR_NONE 0xffffffff
#endif

#define PATH_RESOLV_CONF	"/etc/resolv.conf"
#define PATH_HOSTS			"/etc/hosts"

typedef std::function<void(void* arg, int status, unsigned char* abuf, int alen)> ares_cb;
typedef std::function<void(void* arg, int status, struct hostent* hostent)> ares_host_cb

namespace sylar {
	struct AresOptions {
		int 	m_flags = 16;
		int 	m_timeout = -1;
		int 	m_tries = 2;
		int 	m_ndots = -1;
		uint16_t m_udp_port = 53; // ns's port default 53
		uint16_t m_tcp_port = 53;
		struct in_addr*	servers = NULL;
		int 	m_nservers = -1;
		std::string m_lookups = NULL;
		std::vector<std::string> m_domains = {};
	};

	class SendRequest { // Only use for tcp
	public:
		typedef std::shared_ptr<SendRequest> ptr;
		const unsigned char* data; //Remaining data to send
		size_t len;
	};

	class ServerState {
	public:
		typedef std::shared_ptr<ServerState> ptr;
		ServerState();

		int getTCPFd() const { return tcp_socket; }
		int getUDPFd() const { return udp_socket; }
		int openTcpSocket(uint16_t port);
		int openUdpSocket(uint16_t port);
		int aresFds(fd_set* read_fds, fd_set* write_fds);

	private:
		int udp_socket;
		int tcp_socket;

		/* Mini-buffer for reading the length word */
		unsigned char tcp_lenbuf[2];
		int tcp_lenbuf_pos;
		int tcp_length;

		std::vector<unsigned char> tcp_buffer;
		int tcp_buffer_pos;

		/* TCP output queue */
		std::list<SendRequest::ptr>  m_sends;

		struct in_addr addr; // init by read resolve.conf
	};

	class Query {
	public:
	    typedef std::shared_ptr<Query> ptr;
		uint16_t 		qid;
		time_t 	 		timeout;
		int 			qlen;

		std::string		m_tcpbuf;
		const unsigned char* qbuf; //Arguments passed to ares_send() (qbuf points into tcpbuf)

		std::vector<int> skip_server;
		ares_callback 	callback;

		int 			try_count;
		int 			server_idx; // which one in resolve.conf
		int 			using_tcp;
		int 			error_status;
	};

	/* An IP address pattern; matches an IP address X if X & mask == addr */
	struct apattern {
		struct in_addr addr;
		struct in_addr mask;
	};

	class AresChannel : public AresOptions {
	public:
		typedef std::shared_ptr<AresChannel> ptr;
		typedef RWMutex RWMutexType;
		AresChannel();
		~AresChannel() {}
		void init();
		std::vector<ServerState>& getServer() const { return m_servers; }
		int getServerSize() { return m_servers.size(); }

		int AresChannel::aresRegistFds();

		void AresCallBack();
		void nextServer(Query::ptr query, time_t now);
		void ares_send(Channel::ptr channel, Query::ptr query, time_t now);
		void aresSend(std::vector<unsigned char>& qbuf);
		void aresQuery(const char* name, int dnsclass, int type);
		void aresGethostbyname(const char* name, int family);

		struct timeval *ares_timeout(ares_channel channel, struct timeval *maxtv, struct timeval *tvbuf);
		void ares_process(ares_channel channel, fd_set *read_fds, fd_set *write_fds);
		int ares_mkquery(const char* name, int dnsclass, int type, unsigned short id, int rd);
		int aresExpandName(const unsigned char *encoded, const unsigned char *abuf, int alen, char **s, long *enclen);
		int aresParseReply(const unsigned char *abuf, int alen, struct hostent **host);
		int aresParsePtrReply(const unsigned char *abuf, int alen, const void *addr, int addrlen, int family, struct hostent **host);

		void ares__close_sockets(struct server_state *server);

		void readTcpData(fd_set *read_fds, time_t now);
		void readUdpPackets(fd_set *read_fds, time_t now);
		void processAnswer(unsigned char *abuf, int alen, int whichserver, int tcp, int now);
		void endQuery(Query::ptr query, int status, unsigned char* abuf, int alen);

		struct apattern* sortlist;
		int 				nsort;
	private:
		RWMutexType			m_mutex;
		uint16_t 			next_id;
		std::vector<ServerState::ptr> 		m_servers;
		std::unordered_map<uint16_t, Query::ptr> m_queries;
	};

}

#endif