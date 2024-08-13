#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/net.h>
#include <os/time.h>
#include <printk.h>

int do_net_send(void *txpacket, int length)
{
	// TODO: [p5-task1] Transmit one network packet via e1000 device
	// TODO: [p5-task3] Call do_block when e1000 transmit queue is full
	// TODO: [p5-task3] Enable TXQE interrupt if transmit queue is full

	return e1000_transmit(txpacket, length); // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
	// TODO: [p5-task2] Receive one network packet via e1000 device
	// TODO: [p5-task3] Call do_block when there is no packet on the way

	int offset = 0;

	for (int i = 0; i < pkt_num; ++i) {
		pkt_lens[i] = e1000_poll(rxbuffer + offset);
		offset += pkt_lens[i];
	}

	return offset; // Bytes it has received
}

void e1000_handle_txqe()
{
	spin_lock_acquire(&send_block_queue_lock);

	for (list_node_t *node_ptr = send_block_queue.next;
	     node_ptr != &send_block_queue;) {
		pcb_t *pcb_ptr = NODE2PCB(node_ptr);
		list_node_t *next_node_ptr = node_ptr->next;
		if (spin_lock_try_acquire(&pcb_ptr->lock)) {
			pcb_ptr->status = TASK_READY;
			list_del(node_ptr);
			spin_lock_acquire(&ready_queue_lock);
			list_push(&ready_queue, node_ptr);
			spin_lock_release(&ready_queue_lock);
			spin_lock_release(&pcb_ptr->lock);
		}
		node_ptr = next_node_ptr;
	}

	if (list_empty(&send_block_queue)) {
		e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
	} else {
		e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
	}
	local_flush_dcache();

	spin_lock_release(&send_block_queue_lock);
}

void e1000_handle_rxdmt0()
{
	spin_lock_acquire(&recv_block_queue_lock);

	for (list_node_t *node_ptr = recv_block_queue.next;
	     node_ptr != &recv_block_queue; node_ptr = node_ptr->next) {
		pcb_t *pcb_ptr = NODE2PCB(node_ptr);
		list_node_t *next_node_ptr = node_ptr->next;
		if (spin_lock_try_acquire(&pcb_ptr->lock)) {
			pcb_ptr->status = TASK_READY;
			list_del(node_ptr);
			spin_lock_acquire(&ready_queue_lock);
			list_push(&ready_queue, node_ptr);
			spin_lock_release(&ready_queue_lock);
			spin_lock_release(&pcb_ptr->lock);
		}
		node_ptr = next_node_ptr;
	}

	spin_lock_release(&recv_block_queue_lock);
}

void net_handle_irq(void)
{
	// TODO: [p5-task3] Handle interrupts from network device
	local_flush_dcache();
	uint64_t ICR = e1000_read_reg(e1000, E1000_ICR);
	uint64_t IMS = e1000_read_reg(e1000, E1000_IMS);
	if ((E1000_ICR_TXQE & ICR) && (E1000_IMS_TXQE & IMS)) {
		e1000_handle_txqe();
	}
	if ((E1000_ICR_RXDMT0 & ICR) && (E1000_IMS_RXDMT0 & IMS)) {
		e1000_handle_rxdmt0();
	}
}

void init_stream_list_node(stream_list_node_t *data)
{
	data->len = 0;
	data->next = -1;
	data->prev = -1;
	data->seq = 0;
	data->valid = 0;
}

void init_stream_data()
{
	stream_data_head = stream_data;
	for (int i = 0; i < STREAM_DATA_SIZE; ++i) {
		init_stream_list_node(&stream_data[i]);
	}
	stream_data_head->next = 0;
	stream_data_head->prev = 0;
}

void insert_stream_data(int seq, int len)
{
	int i = stream_data_head->next;
	for (; i != -1 && stream_data[i].valid && stream_data[i].seq < seq &&
	       &stream_data[i] != stream_data_head;
	     i = stream_data[i].next)
		;
	int j = 1;
	for (; j < STREAM_DATA_SIZE; ++j) {
		if (stream_data[j].valid == 0) {
			break;
		}
	}
	stream_data[j].prev = stream_data[i].prev;
	stream_data[i].prev = j;
	stream_data[stream_data[j].prev].next = j;
	stream_data[j].next = i;
	stream_data[j].valid = 1;
	stream_data[j].seq = seq;
	stream_data[j].len = len;
}

int merge_stream_data()
{
	int size = 0;
	for (int i = stream_data_head->next;
	     i != -1 && stream_data[i].valid == 1 &&
	     &stream_data[i] != stream_data_head;
	     i = stream_data[i].next) {
		int l = stream_data[i].seq;
		int r = stream_data[i].len + l;
		int j = stream_data[i].next;
		for (; j != -1 && stream_data[j].valid == 1 &&
		       &stream_data[j] != stream_data_head;
		     j = stream_data[j].next) {
			if (stream_data[j].seq <= r) {
				if (r <
				    stream_data[j].seq + stream_data[j].len) {
					r = stream_data[j].seq +
					    stream_data[j].len;
				}
			} else {
				break;
			}
		}
		size += r - l;
		stream_data[i].len = r - l;
		for (int k = stream_data[i].next; k != j;) {
			int next_k = stream_data[k].next;
			init_stream_list_node(&stream_data[k]);
			k = next_k;
		}
		stream_data[i].next = j;
		stream_data[j].prev = i;
	}
	return size;
}

int ACK_ptr = 0;
int RSD_cnt = 0;

int do_net_recv_stream(void *rxbuffer, int len)
{
	ACK_ptr = 0;
	RSD_cnt = 0;
	init_stream_data();
	resend_time = 0;
	while (merge_stream_data() < len) {
		do_ACK_with_intervals();
		e1000_poll_stream(tmp_buffer);
		char magic = tmp_buffer[PROTOCOL_START];
		char mode = tmp_buffer[PROTOCOL_START + 1];
		if (magic == 0x45 && ((mode & DAT) != 0)) {
			memcpy((uint8_t *)tx_buff, (uint8_t *)tmp_buffer, 54);

			short len0 = tmp_buffer[PROTOCOL_START + 2];
			short len1 = tmp_buffer[PROTOCOL_START + 3];
			short len = (len0 << 8) | len1;

			int seq0 = tmp_buffer[PROTOCOL_START + 4];
			int seq1 = tmp_buffer[PROTOCOL_START + 5];
			int seq2 = tmp_buffer[PROTOCOL_START + 6];
			int seq3 = tmp_buffer[PROTOCOL_START + 7];
			int seq = (seq0 << 24) | (seq1 << 16) | (seq2 << 8) |
				  seq3;

			memcpy((uint8_t *)(rxbuffer + seq),
			       (uint8_t *)(tmp_buffer + PROTOCOL_START + 8),
			       len);
			// printl("receive seq=%d, len=%d\n", seq, len);
			insert_stream_data(seq, len);
		}
	}
	do_ACK();
	return len;
}

void do_RSD()
{
	tx_buff[PROTOCOL_START] = 0x45;
	tx_buff[PROTOCOL_START + 1] = RSD;
	int seq = 0;
	int idx = stream_data_head->next;
	if (stream_data[idx].valid == 1 && stream_data[idx].seq == 0) {
		seq = stream_data[idx].len;
	}
	// printl("send RSD seq=%d\n", seq);
	tx_buff[PROTOCOL_START + 4] = seq >> 24;
	tx_buff[PROTOCOL_START + 5] = (seq & 0x00ff0000) >> 16;
	tx_buff[PROTOCOL_START + 6] = (seq & 0x0000ff00) >> 8;
	tx_buff[PROTOCOL_START + 7] = seq & 0x000000ff;
	e1000_transmit(tx_buff, 62);
}

void do_ACK()
{
	tx_buff[PROTOCOL_START] = 0x45;
	tx_buff[PROTOCOL_START + 1] = ACK;
	int seq = 0;
	int idx = stream_data_head->next;
	if (stream_data[idx].valid == 1 && stream_data[idx].seq == 0 &&
	    stream_data[idx].len > ACK_ptr) {
		RSD_cnt = 0;
		seq = stream_data[idx].len;
		ACK_ptr = seq;
	}
	seq = ACK_ptr;
	// printl("send ACK seq=%d\n", seq);
	tx_buff[PROTOCOL_START + 4] = seq >> 24;
	tx_buff[PROTOCOL_START + 5] = (seq & 0x00ff0000) >> 16;
	tx_buff[PROTOCOL_START + 6] = (seq & 0x0000ff00) >> 8;
	tx_buff[PROTOCOL_START + 7] = seq & 0x000000ff;
	e1000_transmit(tx_buff, 62);
	if (RSD_cnt >= 299) {
		do_RSD();
		RSD_cnt = 0;
	} else {
		++RSD_cnt;
	}
}

void do_ACK_with_intervals() {
	if (get_ns_timer() >= resend_time) {
		do_ACK();
		resend_time = get_ns_timer() + RESEND_INTERVAL;
	}
}

int do_net_send_protocol(void *rxbuffer, int len) {
	uint32_t seq = 0;
	uint8_t fixed_smac[ETH_ALEN] = { 0x80, 0xfa, 0x5b, 0x33, 0x56, 0xef };
    uint8_t fixed_dmac[ETH_ALEN] = { 0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53 }; 
	int ret = 0, pl_len = 0;
	int last_pkt = 0;
	char *pkt = tmp_buffer;
	for (int i = 0; i < len; i = i + 992) {
		memset(pkt, 0, 2048);

		int hdr_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE;

		if (i + 992 < len) {
			pl_len = 1000;
			last_pkt = 0;
		} else {
			pl_len = 8 + len - i;
			last_pkt = 1;
		}

		int pkt_len = hdr_len + pl_len;

		// Assembly ether header
		struct ethhdr *eth_hdr = packet_to_ether_hdr(pkt);

		memcpy(eth_hdr->ether_dhost, fixed_dmac, ETH_ALEN);
    	memcpy(eth_hdr->ether_shost, fixed_smac, ETH_ALEN);
    	eth_hdr->ether_type = htons(ETH_P_IP);

		// Assembly ip header
    	struct iphdr *ip_hdr = packet_to_ip_hdr(pkt);

    	ip_hdr->ihl = 5;
    	ip_hdr->version = 4;
    	ip_hdr->tos = 0;
    	ip_hdr->tot_len = htons(IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE + pl_len);
    	ip_hdr->id = htons(54321);
    	ip_hdr->frag_off = htons(IP_DF);
    	ip_hdr->ttl = DEFAULT_TTL;
    	ip_hdr->protocol = IPPROTO_TCP;
    	ip_hdr->saddr = htonl(10 << 24 |   0 << 16 |   0 << 8 |  67);
    	ip_hdr->daddr = htonl(255 << 24 | 255 << 16 | 255 << 8 | 255);

		// Assembly tcp header and payload
    	struct tcphdr *tcp_hdr = packet_to_tcp_hdr(pkt);

    	tcp_hdr->sport = htons(46930);
    	tcp_hdr->dport = htons(50001);
    	tcp_hdr->seq = htonl(seq++);
    	tcp_hdr->ack = htonl(0);
    	tcp_hdr->off = TCP_HDR_OFFSET;
    	tcp_hdr->flags = TCP_PSH | TCP_ACK;
    	tcp_hdr->rwnd = htons(TCP_DEFAULT_WINDOW);

		protocol_head_t *head = (protocol_head_t *)(pkt + hdr_len);
		head->magic = 0x45;
		head->flag = 0;
		if (last_pkt) {
			head->flag = EOF;
		} else {
			head->flag = 0;
		}
		head->len = htons((uint16_t)pl_len - 8);
		head->seq = htonl(i);
		memcpy((uint8_t *)pkt + PROTOCOL_START + 8, (uint8_t *)rxbuffer + i, pl_len - 8);

		tcp_hdr->checksum = tcp_checksum(ip_hdr, tcp_hdr);
    	ip_hdr->checksum = ip_checksum(ip_hdr);

		e1000_transmit(pkt, pkt_len);
	}
	return ret;
}

int do_net_recv_protocol(void *rxbuffer) {
	ACK_ptr = 0;
	RSD_cnt = 0;
	init_stream_data();
	resend_time = 0;
	while (true) {
		// do_ACK_with_intervals();
		e1000_poll_stream(tmp_buffer);
		char magic = tmp_buffer[PROTOCOL_START];
		char mode = tmp_buffer[PROTOCOL_START + 1];
		if (magic == 0x45) {
			short len0 = tmp_buffer[PROTOCOL_START + 2];
			short len1 = tmp_buffer[PROTOCOL_START + 3];
			short len = (len0 << 8) | len1;

			int seq0 = tmp_buffer[PROTOCOL_START + 4];
			int seq1 = tmp_buffer[PROTOCOL_START + 5];
			int seq2 = tmp_buffer[PROTOCOL_START + 6];
			int seq3 = tmp_buffer[PROTOCOL_START + 7];
			int seq = (seq0 << 24) | (seq1 << 16) | (seq2 << 8) |
				  seq3;

			memcpy((uint8_t *)(rxbuffer + seq),
			       (uint8_t *)(tmp_buffer + PROTOCOL_START + 8),
			       len);
			insert_stream_data(seq, len);
		}
		if (mode == EOF)
			break;
	}
	return stream_data[stream_data_head->next].len;
}

uint16_t ntohs(uint16_t x) {
	uint16_t ret = 0;
	uint8_t *ptr = (uint8_t *)&x;
	ret = (uint16_t)(*ptr) << 8 | (uint16_t)(*(ptr + 1));
	return ret;
}

uint16_t htons(uint16_t x) {
	return ntohs(x);
}

uint32_t htonl(uint32_t x) {
    uint32_t ret = 0;
    uint8_t *ptr = (uint8_t *)&x;
    ret =   (uint32_t)(*ptr) << 24 |
            (uint32_t)(*(ptr + 1)) << 16 |
            (uint32_t)(*(ptr + 2)) << 8 |
            (uint32_t)(*(ptr + 3));
    return ret;
}