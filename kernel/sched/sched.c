#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/loader.h>
#include <os/string.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <csr.h>

extern void ret_from_exception();
pcb_t pcb[NUM_MAX_TASK];
tcb_t tcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb0 = { .pid = 0,
		    .kernel_sp = (ptr_t)pid0_stack,
		    .user_sp = (ptr_t)pid0_stack };
pcb_t pid0_pcb1 = { .pid = 0,
		    .kernel_sp = (ptr_t)pid0_stack + PAGE_SIZE,
		    .user_sp = (ptr_t)pid0_stack + PAGE_SIZE };

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t *volatile current_running;

/* global process id */
pid_t process_id = 1;

pid_t thread_id = 1;

void update_current_running()
{
	asm volatile("mv %0, tp;" : "=r"(current_running));
}

void do_scheduler(void)
{
	// TODO: [p2-task3] Check sleep queue to wake up PCBs
begin:
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
		goto begin; // do nothing
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
	NODE2PCB(pcb_node)->status = TASK_BLOCKED;
	list_push(queue, pcb_node);
}

void do_unblock(list_node_t *pcb_node)
{
	// TODO: [p2-task2] unblock the `pcb` from the block queue
	NODE2PCB(pcb_node)->status = TASK_READY;
	list_del(pcb_node);
	list_push(&ready_queue, pcb_node);
}

static void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack,
			   ptr_t entry_point, pcb_t *pcb, int argc,
			   char *argv[])
{
	// [p3-task1] set user stack for argv
	char **argvStack = (char **)(user_stack - 8 * (argc + 1));
	pcb->user_sp = (reg_t)argvStack;
	for (int i = 0; i < argc; ++i) {
		pcb->user_sp -= strlen(argv[i]) + 1;
		argvStack[i] = (char *)pcb->user_sp;
		strcpy(argvStack[i], argv[i]);
	}
	argvStack[argc] = NULL;
	bzero((void *)(pcb->user_sp - (pcb->user_sp % 16)), pcb->user_sp % 16);
	pcb->user_sp -= (pcb->user_sp % 16);
	/* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
	regs_context_t *pt_regs =
		(regs_context_t *)(kernel_stack - sizeof(regs_context_t));
	for (int i = 0; i < 32; ++i)
		pt_regs->regs[i] = 0;
	pt_regs->regs[2] = (reg_t)pcb->user_sp; // sp
	pt_regs->regs[4] = (reg_t)pcb; // tp
	pt_regs->regs[1] = (reg_t)(entry_point + 2);
	pt_regs->regs[10] = (reg_t)argc;
	pt_regs->regs[11] = (reg_t)argvStack;
	// When a trap is taken, SPP is set to 0 if the trap originated from user mode, or 1 otherwise.
	pt_regs->sstatus = (reg_t)SR_SPIE & ~SR_SPP;
	pt_regs->sepc = (reg_t)entry_point;
	pt_regs->sbadaddr = (reg_t)0;
	pt_regs->scause = (reg_t)0;
	/* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
	switchto_context_t *pt_switchto =
		(switchto_context_t *)((ptr_t)pt_regs -
				       sizeof(switchto_context_t));
	for (int i = 0; i < 14; ++i)
		pt_switchto->regs[i] = (reg_t)0;
	// [p2-task1]
	// pt_switchto->regs[0] = (reg_t) entry_point;         // ra
	// [p2-task3]
	pt_switchto->regs[0] = (reg_t)ret_from_exception; // ra
	pt_switchto->regs[1] = (reg_t)pt_switchto; // sp

	pcb->kernel_sp = (reg_t)pt_switchto;
}

#ifdef S_CORE
pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
}
#else
pid_t do_exec(char *name, int argc, char *argv[])
{
	uint64_t entrypoint = getEntrypoint(name);
	if (entrypoint == 0) {
		printk("Can not find %s!\n", name);
		return 0;
	}
	int pcbidx = -1;
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		if (pcb[i].status == TASK_EXITED) {
			pcbidx = i;
			break;
		}
	}
	assert(pcbidx != -1);
	init_pcb_stack(pcb[pcbidx].kernel_stack_base,
		       pcb[pcbidx].user_stack_base, getEntrypoint(name),
		       pcb + pcbidx, argc, argv);
	pcb[pcbidx].status = TASK_READY;
	pcb[pcbidx].cursor_x = 0;
	pcb[pcbidx].cursor_y = 0;
	pcb[pcbidx].pid = process_id++;
	pcb[pcbidx].tid = 0;
	pcb[pcbidx].wakeup_time = 0;
	list_push(&ready_queue, &pcb[pcbidx].list);
	return pcb[pcbidx].pid;
}
#endif

int do_kill(pid_t pid)
{
	int pcbidx = -1;
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		if (pcb[i].pid == pid) {
			pcbidx = i;
			break;
		}
	}
	if (pcbidx == -1)
		return 0;
	assert(&pcb[pcbidx] != current_running);
	while (!list_empty(&pcb[pcbidx].wait_list)) {
		do_unblock(list_front(&pcb[pcbidx].wait_list));
	}
	list_del(&pcb[pcbidx].list);
	do_destroy_pthread_lock(pid);
	pcb[pcbidx].status = TASK_EXITED;
	pcb[pcbidx].cursor_x = 0;
	pcb[pcbidx].cursor_y = 0;
	pcb[pcbidx].kernel_sp = pcb[pcbidx].kernel_stack_base;
	pcb[pcbidx].user_sp = pcb[pcbidx].user_stack_base;
	pcb[pcbidx].pid = 0;
	pcb[pcbidx].tid = 0;
	pcb[pcbidx].wakeup_time = 0;
	return 1;
}

pid_t do_getpid()
{
	return current_running->pid;
}

void do_exit(void)
{
	while (!list_empty(&current_running->wait_list)) {
		do_unblock(list_front(&current_running->wait_list));
	}
	list_del(&current_running->list);
	do_destroy_pthread_lock(current_running->pid);
	current_running->status = TASK_EXITED;
	current_running->cursor_x = 0;
	current_running->cursor_y = 0;
	current_running->kernel_sp = current_running->kernel_stack_base;
	current_running->user_sp = current_running->user_stack_base;
	current_running->pid = 0;
	current_running->tid = 0;
	current_running->wakeup_time = 0;
	do_scheduler();
}

int do_waitpid(pid_t pid)
{
	int pcbidx = -1;
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		if (pcb[i].pid == pid) {
			pcbidx = i;
			break;
		}
	}
	if (pcbidx == -1)
		return 0;
	current_running->status = TASK_BLOCKED;
	do_block(&current_running->list, &pcb[pcbidx].wait_list);
	do_scheduler();
	return pid;
}

void do_process_show()
{
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		if (pcb[i].status != TASK_EXITED) {
			printk("[%d] PID : %d   STATUS : ", i, pcb[i].pid);
			switch (pcb[i].status) {
			case TASK_READY:
				printk("READY\n");
				break;
			case TASK_BLOCKED:
				printk("BLOCKED\n");
				break;
			case TASK_RUNNING:
				printk("RUNNING\n");
				break;
			default:
				assert(0);
			}
		}
	}
}