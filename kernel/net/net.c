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
uint64_t last_ACK_TIME = 0;

int do_net_recv_stream(void *rxbuffer, int len)
{
	ACK_ptr = 0;
	last_ACK_TIME = 0;
	init_stream_data();
	RSD_resend_time = get_timer() + RESEND_INTERVAL;
	ACK_resend_time = get_timer() + RESEND_INTERVAL;
	while (merge_stream_data() < len) {
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
	printl("send RSD seq=%d\n", seq);
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
		last_ACK_TIME = get_timer();
		seq = stream_data[idx].len;
		ACK_ptr = seq;
		printl("send ACK seq=%d\n", seq);
		tx_buff[PROTOCOL_START + 4] = seq >> 24;
		tx_buff[PROTOCOL_START + 5] = (seq & 0x00ff0000) >> 16;
		tx_buff[PROTOCOL_START + 6] = (seq & 0x0000ff00) >> 8;
		tx_buff[PROTOCOL_START + 7] = seq & 0x000000ff;
		e1000_transmit(tx_buff, 62);
	} else if (last_ACK_TIME != 0 &&
		   get_timer() > last_ACK_TIME + 4 * RESEND_INTERVAL) {
		seq = ACK_ptr;
		printl("send ACK seq=%d\n", seq);
		tx_buff[PROTOCOL_START + 4] = seq >> 24;
		tx_buff[PROTOCOL_START + 5] = (seq & 0x00ff0000) >> 16;
		tx_buff[PROTOCOL_START + 6] = (seq & 0x0000ff00) >> 8;
		tx_buff[PROTOCOL_START + 7] = seq & 0x000000ff;
		e1000_transmit(tx_buff, 62);
	}
}