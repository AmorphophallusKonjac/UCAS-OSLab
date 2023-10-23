#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
tcb_t tcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = { .pid = 0,
		   .kernel_sp = (ptr_t)pid0_stack,
		   .user_sp = (ptr_t)pid0_stack };

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t *volatile current_running;

/* global process id */
pid_t process_id = 1;

pid_t thread_id = 1;

void do_scheduler(void)
{
	// TODO: [p2-task3] Check sleep queue to wake up PCBs
	check_sleeping();
	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/

	// TODO: [p2-task1] Modify the current_running pointer.
	pcb_t *prev_running = current_running;
	if (prev_running->status == TASK_RUNNING) {
		list_push(&ready_queue, &prev_running->list);
		prev_running->status = TASK_READY;
	}

	if (list_empty(&ready_queue)) {
		do_scheduler(); // nothing to do
		return;
	}
	current_running = NODE2PCB(list_front(&ready_queue));
	current_running->status = TASK_RUNNING;
	list_pop(&ready_queue);

	bios_set_timer(get_ticks() + TIMER_INTERVAL);
	// TODO: [p2-task1] switch_to current_running
	switch_to(prev_running, current_running);
}

void do_sleep(uint32_t sleep_time)
{
	// TODO: [p2-task3] sleep(seconds)
	// NOTE: you can assume: 1 second = 1 `timebase` ticks
	// 1. block the current_running
	// 2. set the wake up time for the blocked task
	// 3. reschedule because the current_running is blocked.
	current_running->status = TASK_BLOCKED;
	current_running->wakeup_time = get_timer() + sleep_time;
	list_push(&sleep_queue, &current_running->list);
	do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
	// TODO: [p2-task2] block the pcb task into the block queue
	list_push(queue, pcb_node);
}

void do_unblock(list_node_t *pcb_node)
{
	// TODO: [p2-task2] unblock the `pcb` from the block queue
	NODE2PCB(pcb_node)->status = TASK_READY;
	list_del(pcb_node);
	list_push(&ready_queue, pcb_node);
}
