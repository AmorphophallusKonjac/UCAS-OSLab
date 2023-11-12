#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/smp.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <csr.h>

extern void ret_from_exception();
pcb_t pcb[NUM_MAX_TASK];
tcb_t tcb[NUM_MAX_TASK];
cpu_t cpu[NUM_MAX_CPU];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb[2] = { { .pid = 0,
			.kernel_sp = (ptr_t)pid0_stack,
			.user_sp = (ptr_t)pid0_stack,
			.cpuID = 0,
			.cpuMask = 1,
			.status = TASK_IDLE },
		      { .pid = 0,
			.kernel_sp = (ptr_t)pid0_stack + PAGE_SIZE,
			.user_sp = (ptr_t)pid0_stack + PAGE_SIZE,
			.cpuID = 1,
			.cpuMask = 2,
			.status = TASK_IDLE } };

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
// pcb_t *volatile current_running;

/* global process id */
pid_t process_id = 1;

pid_t thread_id = 1;

void list_print(list_head *queue)
{
	for (list_node_t *i = queue->next; i != queue; i = i->next) {
		printl("%d ", NODE2PCB(i)->pid);
	}
	printl("\n");
}

pcb_t *pick_next_task(uint64_t cpuID)
{
	pcb_t *next_task = NULL;
	for (list_node_t *ptr = list_front(&ready_queue);
	     ptr != &ready_queue && ptr != 0; ptr = ptr->next) {
		pcb_t *task = NODE2PCB(ptr);
		if (task->cpuMask & (1 << cpuID)) {
			next_task = task;
			next_task->status = TASK_RUNNING;
			next_task->cpuID = cpuID;
			list_del(ptr);
			break;
		}
	}
	return (next_task == NULL) ? &pid0_pcb[cpuID] : next_task;
}

void do_scheduler(void)
{
	uint64_t cpuID = get_current_cpu_id();
	// TODO: [p2-task3] Check sleep queue to wake up PCBs
	check_sleeping();
	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/

	// TODO: [p2-task1] Modify the current_running pointer.
	pcb_t *prev_running = cpu[cpuID].current_running;
	if (prev_running->status == TASK_RUNNING) {
		list_push(&ready_queue, &prev_running->list);
		prev_running->status = TASK_READY;
	}
	cpu[cpuID].current_running = pick_next_task(cpuID);
	cpu[cpuID].pid = cpu[cpuID].current_running->pid;
	// printl("cpu %d:   switch pid %d   to   pid %d\n", get_current_cpu_id(),
	//        prev_running->pid, current_running->pid);
	// list_print(&ready_queue);
	// printl("\n");
	bios_set_timer(get_ticks() + TIMER_INTERVAL);
	// TODO: [p2-task1] switch_to current_running
	switch_to(prev_running, cpu[cpuID].current_running);
}

void do_sleep(uint32_t sleep_time)
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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
	pcb[pcbidx].cpuMask = current_running->cpuMask;
	list_push(&ready_queue, &pcb[pcbidx].list);
	return pcb[pcbidx].pid;
}
#endif

int do_kill(pid_t pid)
{
	/* TODO: solve problem of kill task running on other core */
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	return current_running->pid;
}

void do_exit(void)
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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
				printk("READY  ");
				break;
			case TASK_BLOCKED:
				printk("BLOCKED");
				break;
			case TASK_RUNNING:
				printk("RUNNING");
				break;
			default:
				assert(0);
			}
			printk("   MASK : %x", pcb[i].cpuMask);
			if (pcb[i].status == TASK_RUNNING) {
				printk("   Running on core %d\n", pcb[i].cpuID);
			} else {
				printk("\n");
			}
		}
	}
}

int do_taskset(pid_t pid, int mask)
{
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		if (pid == pcb[i].pid) {
			pcb[i].cpuMask = mask;
			return 0;
		}
	}
	return 1;
}