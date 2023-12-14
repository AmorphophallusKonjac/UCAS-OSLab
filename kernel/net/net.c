#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);
spin_lock_t send_block_queue_lock;
spin_lock_t recv_block_queue_lock;

int do_net_send(void *txpacket, int length)
{
	// TODO: [p5-task1] Transmit one network packet via e1000 device
	// TODO: [p5-task3] Call do_block when e1000 transmit queue is full
	// TODO: [p5-task3] Enable TXQE interrupt if transmit queue is full
	pcb_t *current_running = get_current_running();
	if (e1000_send_queue_full()) {
		e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
		local_flush_dcache();

		spin_lock_acquire(&current_running->lock);
		current_running->status = TASK_BLOCKED;
		spin_lock_acquire(&send_block_queue_lock);
		do_block(&current_running->list, &send_block_queue);
		spin_lock_release(&send_block_queue_lock);
		do_scheduler();
	}

	return e1000_transmit(txpacket, length); // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
	// TODO: [p5-task2] Receive one network packet via e1000 device
	// TODO: [p5-task3] Call do_block when there is no packet on the way

	pcb_t *current_running = get_current_running();

	int recv_len = 0;

	for (int i = 0; i < pkt_num; ++i) {
		if (e1000_recv_queue_empty()) {
			spin_lock_acquire(&current_running->lock);
			current_running->status = TASK_BLOCKED;
			spin_lock_acquire(&recv_block_queue_lock);
			do_block(&current_running->list, &recv_block_queue);
			spin_lock_release(&recv_block_queue_lock);
			do_scheduler();
		}
		pkt_lens[i] = e1000_poll(rxbuffer);
		rxbuffer += pkt_lens[i];
		recv_len += pkt_lens[i];
	}

	return recv_len; // Bytes it has received
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
	uint64_t scause = e1000_read_reg(e1000, E1000_ICR);
	if (E1000_ICR_TXQE & scause) {
		e1000_handle_txqe();
	}
	if (E1000_ICR_RXDMT0 & scause) {
		e1000_handle_rxdmt0();
	}
}