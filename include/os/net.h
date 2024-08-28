#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

#include <os/list.h>
#include <type.h>

#define PKT_NUM 32

#define STREAM_DATA_SIZE 1024

#define PROTOCOL_START 54

#define DAT 0x1

#define RSD 0x2

#define ACK 0x4

#define EOF 0x8

uint16_t ntohs(uint16_t x);
uint16_t htons(uint16_t x);
uint32_t htonl(uint32_t x);

// checksum
static inline uint16_t checksum(uint16_t *ptr, int nbytes, uint32_t sum)
{
    if (nbytes % 2) {
        sum += ((uint8_t *)ptr)[--nbytes];
    }

    while (nbytes > 0) {
        sum += *ptr++;
        nbytes -= 2;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);

    return (uint16_t)~sum;
}

// eth
#define ETH_ALEN    6       // length of mac address

#define ETH_P_ALL   0x0003  // every packet, only used when tending to receive all packets
#define ETH_P_IP    0x0800  // IP packet 

struct ethhdr {
    uint8_t ether_dhost[ETH_ALEN];  // destination mac address
    uint8_t ether_shost[ETH_ALEN];  // source mac address
    uint16_t ether_type;            // protocol format
};

#define ETHER_HDR_SIZE sizeof(struct ethhdr)

static inline struct ethhdr *packet_to_ether_hdr(const char *packet)
{
    return (struct ethhdr *)packet;
}

// ip
#define DEFAULT_TTL     64
#define IP_DF	        0x4000		// do not fragment

#ifndef IPPROTO_TCP
#define IPPROTO_TCP     6
#endif  // !IPPROTO_TCP

struct iphdr {
    uint8_t ihl : 4;        // length of ip header
    uint8_t version : 4;    // ip version 
    uint8_t tos;            // type of service (usually set to 0)
    uint16_t tot_len;       // total length of ip data  
    uint16_t id;            // ip identifier
    uint16_t frag_off;      // the offset of ip fragment
    uint8_t ttl;            // ttl of ip packet
    uint8_t protocol;       // upper layer protocol, e.g. icmp, tcp, udp 
    uint16_t checksum;      // checksum of ip header
    uint32_t saddr;         // source ip address
    uint32_t daddr;         // destination ip address
};

#define IP_BASE_HDR_SIZE sizeof(struct iphdr)
#define IP_HDR_SIZE(hdr) (hdr->ihl * 4)
#define IP_DATA(hdr)    ((char *)hdr + IP_HDR_SIZE(hdr))

static inline uint16_t ip_checksum(struct iphdr *hdr)
{
    uint16_t tmp = hdr->checksum;
    hdr->checksum = 0;
    uint16_t sum = checksum((uint16_t *)hdr, hdr->ihl * 4, 0);
    hdr->checksum = tmp;

    return sum;
}

static inline struct iphdr *packet_to_ip_hdr(const char *packet)
{
    return (struct iphdr *)(packet + ETHER_HDR_SIZE);
}

// tcp
#define DEFAULT_SPORT	46930
#define DEFAULT_DPORT1  50001
#define DEFAULT_DPORT2  58688

struct tcphdr {
    uint16_t sport;		    // source port 
    uint16_t dport;		    // destination port
    uint32_t seq;			// sequence number
    uint32_t ack;			// acknowledgement number
    uint8_t x2 : 4;			// (unused)
    uint8_t off : 4;			// data offset
    uint8_t flags;
# define TCP_FIN	0x01
# define TCP_SYN	0x02
# define TCP_RST	0x04
# define TCP_PSH	0x08
# define TCP_ACK	0x10
# define TCP_URG	0x20
    uint16_t rwnd;			// receiving window
    uint16_t checksum;		// checksum
    uint16_t urp;			// urgent pointer
} __attribute__((packed));

#define TCP_HDR_OFFSET 5
#define TCP_BASE_HDR_SIZE 20
#define TCP_HDR_SIZE(tcp) (tcp->off * 4)

#define TCP_DEFAULT_WINDOW 65535

static inline struct tcphdr *packet_to_tcp_hdr(const char *packet)
{
    struct iphdr *ip = packet_to_ip_hdr(packet);
    return (struct tcphdr *)((char *)ip + IP_HDR_SIZE(ip));
}

static inline uint16_t tcp_checksum(struct iphdr *ip, struct tcphdr *tcp)
{
    uint16_t tmp = tcp->checksum;
    tcp->checksum = 0;

    uint16_t reserv_proto = ip->protocol;
    uint16_t tcp_len = ntohs(ip->tot_len) - IP_HDR_SIZE(ip);

    uint32_t sum = ip->saddr + ip->daddr + htons(reserv_proto) \
        + htons(tcp_len);
    uint16_t cksum = checksum((uint16_t *)tcp, (int)tcp_len, sum);

    tcp->checksum = tmp;

    return cksum;
}


typedef struct stream_list_node {
	int valid;
	int prev;
	int next;
	int seq;
	int len;
} stream_list_node_t;

typedef struct protocol_head {
	uint8_t  magic;
	uint8_t  flag;
	uint16_t len;
	uint32_t seq;
} protocol_head_t;

// TCP_PAYLOAD can not be bigger than TCP_MSS
#define TCP_MSS 1460

#define TCP_PAYLOAD TCP_MSS

#define PROTOCOL_HEAD_LEN sizeof(protocol_head_t)

#define PROTOCOL_DATA_LEN TCP_PAYLOAD-PROTOCOL_HEAD_LEN

// stream[0] is head and is always invalid
stream_list_node_t stream_data[STREAM_DATA_SIZE];

stream_list_node_t *stream_data_head;

char tmp_buffer[2048];
char tx_buff[128];

void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);
int do_net_recv_stream(void *rxbuffer, int len);
int do_net_send_protocol(void *rxbuffer, int len);
int do_net_recv_protocol(void *rxbuffer); 
void do_ACK_with_intervals();
void do_RSD();
void do_ACK();

#endif // !__INCLUDE_NET_H__