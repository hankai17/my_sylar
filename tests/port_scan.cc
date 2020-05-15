#include "my_sylar/log.hh"
#include "my_sylar/iomanager.hh"
#include "my_sylar/address.hh"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <map>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_PCKT_LEN 8192
#define IP_PCKT_MAX_LEN 65536
#define PORT_RANGE 65536
#define COMMS_PORT "9527"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
int g_sockfd = -1;
int discovered_ports[PORT_RANGE] = {0};
char* dest_host_name = (char*)"172.16.3.17";
//char* dest_host_name = (char*)"www.ifeng.com";
char* eth_name = (char*)"ens33";

struct my_iph {
        //45 00 00880e4d40004006cd61ac100311ac100390fb560016ac95892c47b27ffb50183f79e8d80000625707156f00e039198f608ab6fb2bada1d59e461b60312d4ea00c49957785673cc371f1e433c9dd173382bad44922455cd1c0eb9f52b41f7c8c35620c7c26d4d9bb96a3013664c1caba00a53b947791c7cf57b8bd349889bbb0dc89f3d87db9
        //eg: 收到45 放到buffer里[0001 0101] 之所以是0x45是因为 5是低位4是高位 so结构体里version放单字节中高位
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t 	hdr_len:        4;      /* in multiples of 5 */
    uint8_t		version:        4;      /* 4 for IPv4, 6 for IPv6 */
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    uint8_t		version:        4;      /* 4 for IPv4, 6 for IPv6 */
	uint8_t 	hdr_len:        4;      /* in multiples of 5 */
#endif

    /*
                --------IP Type of Service--------

                    Precedence
        111 - Network Control
        110 - Internetwork Control
        101 - CRITIC/ECP
        100 - Flash Override
        011 - Flash
        010 - Immediate
        001 - Priority
        000 - Routine

            Throughput
        0 = Normal Delay
        1 = Low Delay

            Reliability
        0 = Normal Throughput
        1 = High Throughput
    */
/* Can simplify this by just using one var to store flags but trying this for fun */
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t		tos_reserved:	 	2;
    uint8_t		tos_reliability: 	1;
    uint8_t		tos_throughput: 	1;
    uint8_t         tos_delay:              1;
    uint8_t		tos_precedence: 	3;
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    uint8_t		tos_precedence: 	3;
        uint8_t         tos_delay:              1;
	uint8_t		tos_throughput: 	1;
	uint8_t		tos_reliability: 	1;
	uint8_t		tos_reserved:	 	2;
#endif

    uint16_t	tot_len;
    uint16_t	identification;		/* used to aid in assembling the fragments of a datagram */

#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t		flg_rsrvd: 	1;	/* must be zero */
    uint8_t		flg_DF: 	1;	/* Don't Fragment */
    uint8_t		flg_MF: 	1;	/* More Fragments */
    uint16_t 	frg_offset: 	13;	/* measured in units of 8 octets (64 bits).  The first fragment has offset zero. */
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    uint8_t		flg_rsrvd: 	1;	/* must be zero */
	uint8_t		flg_DF: 	1;	/* Don't Fragment */
	uint8_t		flg_MF: 	1;	/* More Fragments */
	uint16_t 	frg_offset: 	13;	/* measured in units of 8 octets (64 bits).  The first fragment has offset zero. */
#endif

    uint8_t		ttl;
    uint8_t		protocol;	/* next level protocol used in the data portion of the internet datagram. */
    uint16_t	hdr_chk_sum;	/* checksum of header only */

    uint32_t	src_addr;
    uint32_t	dst_addr;

    /* optional data & padding should be below */
};

struct my_tcph {
    uint16_t	src_port;
    uint16_t	dst_port;

    uint32_t	seq_no;
    uint32_t	ack_no;

# if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t		rsvrd1:		4;	/* must be zero */
    uint8_t		data_offset:	4;
#endif

# if __BYTE_ORDER == __BIG_ENDIAN
    uint8_t		data_offset: 	4;
	uint8_t		rsvd1:		4;
#endif
    /*
                flags
        ---------------------------------------------------
            Control Bits:  6 bits (from left to right):
        URG:  Urgent Pointer field significant
        ACK:  Acknowledgment field significant
        PSH:  Push Function
        RST:  Reset the connection
        SYN:  Synchronize sequence numbers
        FIN:  No more data from sender
    */
/* Can simplify this by just using one var to store flags but trying this for fun */
# if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t		fin:	1;
    uint8_t		syn:	1;
    uint8_t		rst:	1;
    uint8_t		psh:	1;
    uint8_t		ack:	1;
    uint8_t		urg:	1;
    uint8_t		rsvrd2:	2;
#endif

# if __BYTE_ORDER == __BIG_ENDIAN
    uint8_t		rsvrd2:	2;
	uint8_t		urg:	1;
	uint8_t		ack:	1;
	uint8_t		psh:	1;
	uint8_t		rst:	1;
	uint8_t		syn:	1;
	uint8_t		fin:	1;      /* need to look into padding (reserved as total is 6 bits) */
#endif

    uint16_t	window;
    uint16_t	chksum;		/* The checksum also covers a 96 bit pseudo header
						conceptually prefixed to the TCP header. */
    uint16_t        urg_ptr;

    /* Options and data should be added below. */
};

struct psuedo_header {
    uint32_t 	src_addr;
    uint32_t 	dst_addr;
    uint8_t 	rsvd;
    uint8_t 	proto;
    uint16_t 	len_tcp;
};

/* Hacky code; need to find a better way */
bool INTERFACE_PRINTED = false;
bool TARGET_RESOLVED =	false;

void set_interface_ip(const char* interface_name, struct my_iph* snd_iph) {
    std::multimap<std::string, std::pair<sylar::Address::ptr, uint32_t> > result;
    if (!sylar::Address::GetInterfaceAddresses(result, AF_INET)) {
        SYLAR_LOG_DEBUG(g_logger) << "GetInterfaceAddresses failed";
        //return;
    } else {
        for (const auto& i : result) {
            //SYLAR_LOG_DEBUG(g_logger) << i.first << " " << i.second.first->toString() << " " << i.second.second;
            if (strcmp(i.first.c_str(), interface_name) == 0) {
                if (INTERFACE_PRINTED == false) {
                    SYLAR_LOG_DEBUG(g_logger) << "Interface: " << i.first
                      << " Address: " << i.second.first->toString();
                    INTERFACE_PRINTED = true;

                    struct sockaddr_in sa;
                    inet_pton(AF_INET, i.second.first->toString().c_str(), &(sa.sin_addr));
                    snd_iph->src_addr = sa.sin_addr.s_addr;
                }
            }
        }
    }
}

void set_dest_ip(struct my_iph* snd_iph) {
    std::vector<sylar::Address::ptr> result;
    if (!sylar::Address::Lookup(result, dest_host_name, 
            AF_INET, SOCK_STREAM, 0)) {
        SYLAR_LOG_DEBUG(g_logger) << "Can not get " << dest_host_name << " ip";
        return;
    } else {
        for (const auto& i : result) {
            SYLAR_LOG_DEBUG(g_logger) << "[*] Destination's IP address: " << i->toString();
            //snd_iph->dst_addr = ipv4->sin_addr.s_addr;
            snd_iph->dst_addr = (*((struct sockaddr_in *)(i->getAddr()))).sin_addr.s_addr;
        }
    }
}

void set_ip_hdr(struct my_iph* snd_iph) {
    snd_iph->version = 4;			/* IPv4 */
    snd_iph->hdr_len = 5;			/* (5 * 4) = 20 bytes; no options */

    snd_iph->tos_precedence = 0x0;		// /* priority */
    snd_iph->tos_throughput = 0x0;		// /* low delay */
    snd_iph->tos_reliability = 0x0;		// /* high priority */
    snd_iph->tos_reserved = 0x0;

    snd_iph->tot_len = sizeof(struct my_iph) + sizeof(struct my_tcph);	/* No payload */
    snd_iph->identification = htons(54321); // test value;

    snd_iph->flg_rsrvd = 0x0;
    snd_iph->flg_DF = 0x0;
    snd_iph->flg_MF = 0x0;

    snd_iph->frg_offset = 0; 			/* Don't fragment */
    snd_iph->ttl = 255;				/* Max TTL */
    snd_iph->protocol = IPPROTO_TCP;
    snd_iph->hdr_chk_sum = 0;			/* calc later */

    set_interface_ip(eth_name, snd_iph);

    if (TARGET_RESOLVED == false) {
        set_dest_ip(snd_iph);
        TARGET_RESOLVED = true;
    }
}

void set_tcp_hdr(struct my_tcph* snd_tcph) {
    snd_tcph->src_port = htons(atoi(COMMS_PORT));
    snd_tcph->dst_port = htons(0);		/* Iterate through later */

    snd_tcph->seq_no = 0;
    snd_tcph->ack_no = 0;			/* TCP ack is 0 in first packet */

    snd_tcph->rsvrd1 = 0x0;
    snd_tcph->data_offset = 5;		/* Assuming the minimal header length */

    snd_tcph->syn = 1;
    snd_tcph->urg = 0;
    snd_tcph->psh = 0;
    snd_tcph->rst = 0;
    snd_tcph->fin = 0;
    snd_tcph->ack = 0;

    snd_tcph->window = htons(5840);
    snd_tcph->chksum = 0;			/* Will compute after header is completly set */

    snd_tcph->urg_ptr = 0;
}

uint16_t csum(const void* data, const int length) {
    /*  Checksum Algorithm (http://www.microhowto.info/howto/calculate_an_internet_protocol_checksum_in_c.html)
    1. Set the sum to 0,
    2. Pad the data to an even number of bytes,
    3. Reinterpret the data as a sequence of 16-bit unsigned integers that are
        in network byte order,
    4. Calculate the sum of the integers, subtracting 0xffff whenever
        the sum => 0x10000, and
    5. Calculate the bitwise complement of the sum and set it as the checksum.
    */
    uint16_t *accumalator = (uint16_t *)data;
    uint64_t sum = 0;

    /* Take care of the first 16-bit even blocks */
    for (int i = 0; i < length/2; ++i) {
        sum += *(accumalator+i);
        if (sum >= 0x10000) {
            sum -= 0xffff;
        }
    }

    /* Handle the ending partial block */
    if (length % 2 != 0) {
        accumalator = accumalator+ length/2; /* Point accumalator to the end block */
        uint16_t end_block = 0;
        memcpy(&end_block, accumalator, sizeof(length));
        sum += ntohs(end_block);
        if (sum >= 0x10000) {
            sum -= 0xffff;
        }
    }

    /* Return the one's complement of the checksum in network byte order */
    return htons(~sum);
}

uint16_t tcp_chksum(struct my_iph* snd_iph, struct my_tcph* snd_tcph) {
    struct psuedo_header psh;

    psh.src_addr = snd_iph->src_addr;
    psh.dst_addr = snd_iph->dst_addr;
    psh.rsvd = 0;
    psh.proto = IPPROTO_TCP;
    psh.len_tcp = htons(sizeof(struct my_tcph));	/* No options, and no data */

    int pseudogram_size = sizeof(struct my_tcph) + sizeof(struct psuedo_header);
    char *pseudogram = (char*)malloc(pseudogram_size);

    memcpy(pseudogram, (char *)&psh, sizeof(struct psuedo_header));
    memcpy(pseudogram + sizeof(struct psuedo_header), snd_tcph, sizeof(struct my_tcph));

    return(htons(csum(pseudogram, pseudogram_size)));
    //return (htons(csum(snd_tcph, sizeof(struct my_tcph)) + csum(&psh, sizeof(struct psuedo_header))));
}

void close_connection(uint16_t port, struct sockaddr_storage from_addr) {
    /* Packet to be sent to close a connection to a port  */
    char closing_packet[MAX_PCKT_LEN];
    memset(closing_packet, 0 , MAX_PCKT_LEN);

    /* Struct to send for closing the connection */
    struct my_iph* close_iph = (struct my_iph *)closing_packet;
    struct my_tcph* close_tcph = (struct my_tcph *)(closing_packet + sizeof(struct my_iph));

    set_ip_hdr(close_iph);
    set_tcp_hdr(close_tcph);

    /* Hacky code; need to find a better approach */
    close_iph->dst_addr = ((struct sockaddr_in *)&from_addr)->sin_addr.s_addr;

    /* Close the connection to the port */
    close_tcph->fin = 0x1;
    close_tcph->ack = 0x0;

    close_tcph->dst_port = htons(port);
    close_iph->hdr_chk_sum = csum(closing_packet, close_iph->tot_len);
    close_tcph->chksum = tcp_chksum(close_iph, close_tcph);

    if (sendto(g_sockfd, closing_packet, close_iph->tot_len,
               0, (struct sockaddr *)&from_addr, sizeof(from_addr)) <= 0) {
        perror("sendto()");
    }

    close_tcph->chksum = 0;
}

std::string to_hex(const std::string& str) {
    std::stringstream ss;
    for(size_t i = 0; i < str.size(); ++i) {
        ss << std::setw(2) << std::setfill('0') << std::hex
          << (int)(uint8_t)str[i];
    }
    return ss.str();
}

void do_listen() {
    for (;;) {
        char response_packet[IP_PCKT_MAX_LEN];
        memset(response_packet, 0, IP_PCKT_MAX_LEN);

        struct sockaddr_storage from_addr;
        socklen_t from_len = 0;

        int byte_count = recvfrom(g_sockfd, response_packet, MAX_PCKT_LEN, 0, (struct sockaddr *)&from_addr, &from_len);
        if (byte_count < 0 && errno != EAGAIN) {
            SYLAR_LOG_DEBUG(g_logger) << "recvfrom failed errno: " << errno 
              << " strerror: " << strerror(errno);
            continue;
        }

        struct my_iph* recv_iph = (struct my_iph*)response_packet;
        struct my_tcph* recv_tcph = (struct my_tcph*)(response_packet + 4 * (recv_iph->hdr_len));

        //SYLAR_LOG_DEBUG(g_logger) << "recvfrom str: " << to_hex(std::string(response_packet, byte_count));
        //450000880e4d40004006cd61ac100311ac100390fb560016ac95892c47b27ffb50183f79e8d80000625707156f00e039198f608ab6fb2bada1d59e461b60312d4ea00c49957785673cc371f1e433c9dd173382bad44922455cd1c0eb9f52b41f7c8c35620c7c26d4d9bb96a3013664c1caba00a53b947791c7cf57b8bd349889bbb0dc89f3d87db9
        if (recv_tcph->dst_port != ntohs(atoi(COMMS_PORT))) {
            continue;
        }

        /* Check if we the port is closed (denoted by a rst flag) */
        if (recv_tcph->rst == 0x01) {
            continue;
        }

        /* Check if the target is closing the connection (done if we haven't responded to it's acks) */
        if (recv_tcph->fin == 0x01) {
            continue;
        }

        if (recv_tcph->ack == 0x01) {
            /* Check if ack is retransmitted due to delayed fin */
            if (discovered_ports[ntohs(recv_tcph->src_port) == 1]) {
                continue;
            }
            SYLAR_LOG_DEBUG(g_logger) << "[*] Port: " << ntohs(recv_tcph->src_port);
            discovered_ports[ntohs(recv_tcph->src_port)] = 1;
            //close_connection(ntohs(recv_tcph->src_port), from_addr);
        }
    }
}

void do_scan() {
    char scanning_packet[MAX_PCKT_LEN];
    memset(scanning_packet, 0 , MAX_PCKT_LEN);

    struct my_iph* snd_iph = (struct my_iph *)scanning_packet;
    struct my_tcph* snd_tcph = (struct my_tcph *)(scanning_packet + sizeof(struct my_iph));

    set_ip_hdr(snd_iph);
    set_tcp_hdr(snd_tcph);

    /* Set up the destination address struct */
    struct sockaddr_in p_dest_addr;
    memset((char *)&p_dest_addr, 0, sizeof(struct sockaddr_in));
    p_dest_addr.sin_family = AF_INET;	/* IPv4 address */
    p_dest_addr.sin_port = htons(atoi(COMMS_PORT));
    p_dest_addr.sin_addr.s_addr = snd_iph->dst_addr;

    SYLAR_LOG_DEBUG(g_logger) << "[*] Port Scan Started";

    for (int i = 1; i < 65535; ++i) {
        snd_tcph->dst_port = htons(i);
        snd_iph->hdr_chk_sum = csum(scanning_packet, snd_iph->tot_len);
        snd_tcph->chksum = tcp_chksum(snd_iph, snd_tcph);

        if (sendto(g_sockfd, scanning_packet, snd_iph->tot_len,
                   0, (struct sockaddr *)&p_dest_addr, sizeof(p_dest_addr)) <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << "sendto failed";
        }

        snd_tcph->chksum = 0;
        usleep(100);
    }
    SYLAR_LOG_DEBUG(g_logger) << "port scan done";
    return;
}

void test() {
    g_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    int a = 1; int b = 1;
    if (setsockopt(g_sockfd, IPPROTO_IP, IP_HDRINCL, &a, sizeof(int)) < 0) {
        SYLAR_LOG_DEBUG(g_logger) << "setsockopt failed";
        return;
    }
    if (setsockopt(g_sockfd, SOL_SOCKET, SO_REUSEADDR, &b, sizeof(int)) < 0) {
        SYLAR_LOG_DEBUG(g_logger) << "setsockopt failed";
        return;
    }
    sylar::IOManager::GetThis()->schedule(do_listen);
    sylar::IOManager::GetThis()->schedule(do_scan);

}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(test);
    iom.stop();
    return 0;
}
