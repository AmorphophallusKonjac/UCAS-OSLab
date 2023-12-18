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

typedef struct stream_list_node {
	int valid;
	int prev;
	int next;
	int seq;
	int len;
} stream_list_node_t;

// stream[0] is head and is always invalid
stream_list_node_t stream_data[STREAM_DATA_SIZE];

stream_list_node_t *stream_data_head;

char tmp_buffer[2048];
char tx_buff[128];

void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);
int do_net_recv_stream(void *rxbuffer, int len);
void do_RSD();
void do_ACK();

#endif // !__INCLUDE_NET_H__