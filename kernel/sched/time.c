#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
	__asm__ __volatile__("rdtime %0" : "=r"(time_elapsed));
	return time_elapsed;
}

uint64_t get_timer()
{
	return get_ticks() / time_base;
}

uint64_t get_time_base()
{
	return time_base;
}

void latency(uint64_t time)
{
	uint64_t begin_time = get_timer();

	while (get_timer() - begin_time < time)
		;
	return;
}

void check_sleeping(void)
{
	// TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
	uint64_t current_time = get_timer();
	for (list_node_t *node_ptr = sleep_queue.next;
	     node_ptr != &sleep_queue;) {
		list_node_t *next_node_ptr = node_ptr->next;
		pcb_t *pcb_ptr = NODE2PCB(node_ptr);
		if (pcb_ptr->wakeup_time <= current_time) {
			pcb_ptr->status = TASK_READY;
			list_del(node_ptr);
			list_push(&ready_queue, node_ptr);
		}
		node_ptr = next_node_ptr;
	}
}