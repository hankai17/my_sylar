#ifndef __ARES_HH__
#define __ARES_HH__

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unordered_map>
#include <atomic>
#include "thread.hh"
#include "tcp_server.hh"
#include "address.hh"

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
#define	DEFAULT_TIMEOUT		5
#define DEFAULT_TRIES		4
#ifndef INADDR_NONE
#define	INADDR_NONE 0xffffffff
#endif

typedef enum __ns_opcode {
	ns_o_query = 0,         /* Standard query. */
	ns_o_iquery = 1,        /* Inverse query (deprecated/unsupported). */
	ns_o_status = 2,        /* Name server status query (unsupported). */
	/* Opcode 3 is undefined/reserved. */
			ns_o_notify = 4,        /* Zone change notification. */
	ns_o_update = 5,        /* Zone update message. */
	ns_o_max = 6
} ns_opcode;

typedef enum __ns_class {
	ns_c_invalid = 0,       /* Cookie. */
	ns_c_in = 1,            /* Internet. */
	ns_c_2 = 2,             /* unallocated/unsupported. */
	ns_c_chaos = 3,         /* MIT Chaos-net. */
	ns_c_hs = 4,            /* MIT Hesiod. */
	/* Query class values which do not appear in resource records */
			ns_c_none = 254,        /* for prereq. sections in update requests */
	ns_c_any = 255,         /* Wildcard match. */
	ns_c_max = 65536
} ns_class;

typedef enum __ns_type {
	ns_t_invalid = 0,       /* Cookie. */
	ns_t_a = 1,             /* Host address. */
	ns_t_ns = 2,            /* Authoritative server. */
	ns_t_md = 3,            /* Mail destination. */
	ns_t_mf = 4,            /* Mail forwarder. */
	ns_t_cname = 5,         /* Canonical name. */
	ns_t_soa = 6,           /* Start of authority zone. */
	ns_t_mb = 7,            /* Mailbox domain name. */
	ns_t_mg = 8,            /* Mail group member. */
	ns_t_mr = 9,            /* Mail rename name. */
	ns_t_null = 10,         /* Null resource record. */
	ns_t_wks = 11,          /* Well known service. */
	ns_t_ptr = 12,          /* Domain name pointer. */
	ns_t_hinfo = 13,        /* Host information. */
	ns_t_minfo = 14,        /* Mailbox information. */
	ns_t_mx = 15,           /* Mail routing information. */
	ns_t_txt = 16,          /* Text strings. */
	ns_t_rp = 17,           /* Responsible person. */
	ns_t_afsdb = 18,        /* AFS cell database. */
	ns_t_x25 = 19,          /* X_25 calling address. */
	ns_t_isdn = 20,         /* ISDN calling address. */
	ns_t_rt = 21,           /* Router. */
	ns_t_nsap = 22,         /* NSAP address. */
	ns_t_nsap_ptr = 23,     /* Reverse NSAP lookup (deprecated). */
	ns_t_sig = 24,          /* Security signature. */
	ns_t_key = 25,          /* Security key. */
	ns_t_px = 26,           /* X.400 mail mapping. */
	ns_t_gpos = 27,         /* Geographical position (withdrawn). */
	ns_t_aaaa = 28,         /* Ip6 Address. */
	ns_t_loc = 29,          /* Location Information. */
	ns_t_nxt = 30,          /* Next domain (security). */
	ns_t_eid = 31,          /* Endpoint identifier. */
	ns_t_nimloc = 32,       /* Nimrod Locator. */
	ns_t_srv = 33,          /* Server Selection. */
	ns_t_atma = 34,         /* ATM Address */
	ns_t_naptr = 35,        /* Naming Authority PoinTeR */
	ns_t_kx = 36,           /* Key Exchange */
	ns_t_cert = 37,         /* Certification record */
	ns_t_a6 = 38,           /* IPv6 address (deprecates AAAA) */
	ns_t_dname = 39,        /* Non-terminal DNAME (for IPv6) */
	ns_t_sink = 40,         /* Kitchen sink (experimentatl) */
	ns_t_opt = 41,          /* EDNS0 option (meta-RR) */
	ns_t_tsig = 250,        /* Transaction signature. */
	ns_t_ixfr = 251,        /* Incremental zone transfer. */
	ns_t_axfr = 252,        /* Transfer zone of authority. */
	ns_t_mailb = 253,       /* Transfer mailbox records. */
	ns_t_maila = 254,       /* Transfer mail agent records. */
	ns_t_any = 255,         /* Wildcard match. */
	ns_t_zxfr = 256,        /* BIND-specific, nonstandard. */
	ns_t_max = 65536
} ns_type;

typedef enum __ns_rcode {
	ns_r_noerror = 0,       /* No error occurred. */
	ns_r_formerr = 1,       /* Format error. */
	ns_r_servfail = 2,      /* Server failure. */
	ns_r_nxdomain = 3,      /* Name error. */
	ns_r_notimpl = 4,       /* Unimplemented. */
	ns_r_refused = 5,       /* Operation refused. */
	/* these are for BIND_UPDATE */
			ns_r_yxdomain = 6,      /* Name exists */
	ns_r_yxrrset = 7,       /* RRset exists */
	ns_r_nxrrset = 8,       /* RRset does not exist */
	ns_r_notauth = 9,       /* Not authoritative for zone */
	ns_r_notzone = 10,      /* Zone of record different from zone section */
	ns_r_max = 11,
	/* The following are TSIG extended errors */
			ns_r_badsig = 16,
	ns_r_badkey = 17,
	ns_r_badtime = 18
} ns_rcode;

#define SERVFAIL        ns_r_servfail
#define NOTIMP          ns_r_notimpl
#define REFUSED         ns_r_refused
#undef NOERROR /* it seems this is already defined in winerror.h */
#define NOERROR         ns_r_noerror
#define FORMERR         ns_r_formerr
#define NXDOMAIN        ns_r_nxdomain

#define T_PTR          ns_t_ptr
#define T_A            ns_t_a
#define C_IN           ns_c_in
#define QUERY          ns_o_query
#define MAXLABEL   63
#define INDIR_MASK 0xc0
#define RRFIXEDSZ  10
#define T_CNAME                ns_t_cname

#define PACKETSZ   512     /* maximum packet size */
#define HFIXEDSZ   12
#define QFIXEDSZ   4
#define PATH_RESOLV_CONF	"/etc/resolv.conf"
#define PATH_HOSTS			"/etc/hosts"

typedef std::function<void(void* arg, int status, unsigned char* abuf, int alen)> ares_cb;
typedef std::function<void(void* arg, int status, struct hostent* hostent)> ares_host_cb;

namespace sylar {

	class SendRequest { // Only use for tcp
	public:
		typedef std::shared_ptr<SendRequest> ptr;
		const unsigned char* data; //Remaining data to send
		size_t len;
	};

	class ServerState {
	public:
		typedef std::shared_ptr<ServerState> ptr;
		int getTCPFd() const { return tcp_socket; }
		int getUDPFd() const { return udp_socket; }
		int openTcpSocket(uint16_t port);
		int openUdpSocket(uint16_t port);

		int udp_socket;
		int tcp_socket;

		/* TCP output queue */
		struct in_addr addr; // init by read resolve.conf
	};

	class Query {
	public:
	    typedef std::shared_ptr<Query> ptr;
	    friend class AresChannel;
		uint16_t 		qid;
		time_t 	 		timeout;
		int 			qlen;

		std::vector<uint8_t> m_tcpbuf;
		const unsigned char* qbuf; // qbuf points into tcpbuf

		std::vector<int> skip_server;
		ares_cb 	callback;
		Fiber::ptr		fiber;
		//struct hostent  host;
		std::vector<in_addr> result;

		int 			try_count;
		int 			server_idx; // which one in resolve.conf
		int 			using_tcp;
		int 			error_status;
	};

	class AresChannel : public UdpServer {
	public:
		typedef std::shared_ptr<AresChannel> ptr;
		typedef RWMutex RWMutexType;
		AresChannel(sylar::IOManager* worker = sylar::IOManager::GetThis(),
				sylar::IOManager* receiver = sylar::IOManager::GetThis());
		~AresChannel() {}
		void init();
		void setServers(std::map<int, std::map<Address::ptr, Socket::ptr> >&& v) { m_servers = v; }
		int getServerSize() { return m_servers.size(); }
		int getIdxServerFd(int idx);

		int ares_mkquery(const char* name, int dnsclass, int type, uint16_t id, int rd,
				std::vector<uint8_t>& buf);

		std::vector<IPv4Address> aresGethostbyname(const std::string& name);
		uint16_t aresQuery(const std::string& name, int dnsclass = C_IN, int type = T_A);
		void aresSend(const std::vector<uint8_t>& qbuf);
		void ares_send(Query::ptr query);
		void nextServer(Query::ptr query);

		void processAnswer(uint8_t* abuf, int alen, int whichserver, int tcp);
		int aresParseReply(uint8_t* abuf, int alen, std::vector<in_addr>& addr);
		int aresExpandName(uint8_t* encoded, uint8_t* abuf, int alen, std::vector<uint8_t>& s, int* enclen);

	private:
		static void QueryTimeout(std::weak_ptr<Query> q, AresChannel::ptr channel);

	protected:
		void startReceiver(Socket::ptr sock) override;
		//void handleClient(Socket::ptr client) override;
	private:
		RWMutexType			m_mutex;
		int 				m_flags = 16;
		int 				m_timeout = -1;
		int 				m_tries = 2;
		uint16_t 			m_udp_port = htons(53);
		uint16_t 			m_tcp_port = htons(53);
		std::atomic<uint16_t> 						m_nextId;
		std::map<int, std::map<Address::ptr, Socket::ptr> >	m_servers; // resolv.conf's addr
		std::unordered_map<uint16_t, Query::ptr> 	m_queries;
	};

}

#endif


//Q可以用指针但手动释放台麻烦 所以用智能指针shared_ptr
//Q->fiber用哪个?
//	系统的fiber系统很稳 如果用shared_ptr会不会影响系统  ------肯定: 当ares_test结束时 而此时map<int, Q>忘记把Q摘除 Q没调析构 fiber引用会多一个 导致系统崩溃
// 													当然可以使用schedule fiber*规避 但这不符合代码易读规范 一切应该建立在设计规范的基础之上

