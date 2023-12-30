#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/smp.h>
#include <os/page.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <csr.h>
#include <pgtable.h>
#include <e1000.h>

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

spin_lock_t ready_queue_lock, sleep_queue_lock;
// TODO: use ready_queue_lock

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

pcb_t *pick_next_task(pcb_t *prev_running, uint64_t cpuID)
{
	pcb_t *next_task = NULL;
	spin_lock_acquire(&ready_queue_lock);
	for (list_node_t *ptr = list_front(&ready_queue);
	     ptr != &ready_queue && ptr != 0; ptr = ptr->next) {
		pcb_t *task = NODE2PCB(ptr);
		if (task == prev_running) {
			if (task->cpuMask & (1 << cpuID)) {
				next_task = task;
				next_task->status = TASK_RUNNING;
				next_task->cpuID = cpuID;
				list_del(ptr);
				break;
			}
		} else if (spin_lock_try_acquire(&task->lock)) {
			if (task->cpuMask & (1 << cpuID)) {
				next_task = task;
				next_task->status = TASK_RUNNING;
				next_task->cpuID = cpuID;
				list_del(ptr);
				break;
			}
			spin_lock_release(&task->lock);
		}
	}
	spin_lock_release(&ready_queue_lock);
	return (next_task == NULL) ? &pid0_pcb[cpuID] : next_task;
}

void yield(void)
{
	pcb_t *current_running = get_current_running();
	spin_lock_acquire(&current_running->lock);
	do_scheduler();
}

void do_scheduler(void)
{
	/* assume hold current_running lock before enter do_shceduler */
	pcb_t *current_running = get_current_running();
	uint64_t cpuID = current_running->cpuID;
	// TODO: [p2-task3] Check sleep queue to wake up PCBs
	check_sleeping();
	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/

	// TODO: [p2-task1] Modify the current_running pointer.
	pcb_t *prev_running = current_running;
	if (prev_running->status == TASK_RUNNING) {
		spin_lock_acquire(&ready_queue_lock);
		list_push(&ready_queue, &prev_running->list);
		spin_lock_release(&ready_queue_lock);
		prev_running->status = TASK_READY;
	}
	cpu[cpuID].current_running = pick_next_task(prev_running, cpuID);
	cpu[cpuID].current_running->switch_from = prev_running;
	cpu[cpuID].pid = cpu[cpuID].current_running->pid;
	// printl("cpu %d:   switch pid %d   to   pid %d\n", get_current_cpu_id(),
	//        prev_running->pid, cpu[cpuID].current_running->pid);
	// printl("ready_queue: ");
	// list_print(&ready_queue);
	set_satp(SATP_MODE_SV39, cpu[cpuID].current_running->pid,
		 kva2pa((uintptr_t)cpu[cpuID].current_running->pagedir) >>
			 NORMAL_PAGE_SHIFT);
	local_flush_tlb_all();
	local_flush_icache_all();
	bios_set_timer(get_ticks() + TIMER_INTERVAL);
	// TODO: [p2-task1] switch_to current_running
	switch_to(prev_running, cpu[cpuID].current_running);
	spin_lock_release(&prev_running->switch_from->lock);
	spin_lock_release(&prev_running->lock);
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
	spin_lock_acquire(&current_running->lock);
	current_running->status = TASK_BLOCKED;
	current_running->wakeup_time = get_timer() + sleep_time;
	spin_lock_acquire(&sleep_queue_lock);
	list_push(&sleep_queue, &current_running->list);
	spin_lock_release(&sleep_queue_lock);
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
	spin_lock_acquire(&NODE2PCB(pcb_node)->lock);
	NODE2PCB(pcb_node)->status = TASK_READY;
	list_del(pcb_node);
	spin_lock_acquire(&ready_queue_lock);
	list_push(&ready_queue, pcb_node);
	spin_lock_release(&ready_queue_lock);
	spin_lock_release(&NODE2PCB(pcb_node)->lock);
}

void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
		    pcb_t *pcb, int argc, char *argv[])
{
	// [p3-task1] set user stack for argv
	ptr_t vUserSp = user_stack;
	ptr_t pUserSp = allocPage(pcb->pid, vUserSp - PAGE_SIZE, PINNED);
	map_page(vUserSp - PAGE_SIZE, kva2pa((uintptr_t)pUserSp), pcb->pagedir,
		 pcb->pid);
	pUserSp += PAGE_SIZE;
	char **pArgvStack = (char **)(pUserSp - 8 * (argc + 1));
	char **vArgvStack = (char **)(vUserSp - 8 * (argc + 1));
	pUserSp = (ptr_t)pArgvStack;
	vUserSp = (ptr_t)vArgvStack;
	for (int i = 0; i < argc; ++i) {
		vUserSp -= strlen(argv[i]) + 1;
		pUserSp -= strlen(argv[i]) + 1;
		strcpy((char *)pUserSp, argv[i]);
		pArgvStack[i] = (char *)vUserSp;
	}
	pArgvStack[argc] = NULL;
	bzero((void *)(pUserSp - (pUserSp % 16)), pUserSp % 16);
	pUserSp -= (pUserSp % 16);
	vUserSp -= (vUserSp % 16);
	pcb->user_sp = vUserSp;
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
	pt_regs->regs[1] = (reg_t)(entry_point + 2); // ra
	pt_regs->regs[10] = (reg_t)argc; // a0
	pt_regs->regs[11] = (reg_t)vArgvStack; // a1
	// When a trap is taken, SPP is set to 0 if the trap originated from user mode, or 1 otherwise.
	pt_regs->sstatus = ((reg_t)SR_SPIE & (reg_t)~SR_SPP) | (reg_t)SR_SUM;
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
	// pt_switchto->regs[0] = (reg_t)entry_point;         // ra
	// [p2-task3]
	// pt_switchto->regs[0] = (reg_t)ret_from_exception;   // ra
	// [p3-task5]
	pt_switchto->regs[0] = (reg_t)forkret; // ra
	pt_switchto->regs[1] = (reg_t)pt_switchto; // sp

	pcb->kernel_sp = (reg_t)pt_switchto;
}

void release_lock(void)
{
	pcb_t *current_running = get_current_running();
	spin_lock_release(&current_running->switch_from->lock);
	spin_lock_release(&current_running->lock);
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
		spin_lock_acquire(&pcb[i].lock);
		if (pcb[i].status == TASK_EXITED) {
			pcbidx = i;
			break;
		}
		spin_lock_release(&pcb[i].lock);
	}
	assert(pcbidx != -1);
	pcb[pcbidx].pid = process_id++;
	pcb[pcbidx].pagedir = initPgtable(pcb[pcbidx].pid);
	from_name_load_task_img(name, &pcb[pcbidx]);
	// TODO: fix init_pcb_stack for access for user stack
	init_pcb_stack(pcb[pcbidx].kernel_stack_base + PAGE_SIZE,
		       pcb[pcbidx].user_stack_base + PAGE_SIZE, entrypoint,
		       pcb + pcbidx, argc, argv);
	pcb[pcbidx].status = TASK_READY;
	pcb[pcbidx].cursor_x = 0;
	pcb[pcbidx].cursor_y = 0;
	pcb[pcbidx].tid = 0;
	pcb[pcbidx].wakeup_time = 0;
	pcb[pcbidx].cpuMask = current_running->cpuMask;
	pcb[pcbidx].wd_inum = current_running->wd_inum;
	strcpy(pcb[pcbidx].wd, current_running->wd);
	spin_lock_acquire(&ready_queue_lock);
	list_push(&ready_queue, &pcb[pcbidx].list);
	spin_lock_release(&ready_queue_lock);
	printl("ready_queue: ");
	list_print(&ready_queue);
	spin_lock_release(&pcb[pcbidx].lock);
	return pcb[pcbidx].pid;
}
#endif

int do_kill(pid_t pid)
{
	int pcbidx = -1;
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		spin_lock_acquire(&pcb[i].lock);
		if (pcb[i].pid == pid) {
			pcbidx = i;
			break;
		}
		spin_lock_release(&pcb[i].lock);
	}
	if (pcbidx == -1)
		return 0;
	pcb[pcbidx].killed = 1;
	spin_lock_release(&pcb[pcbidx].lock);
	return 1;
}

pid_t do_getpid()
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	return current_running->pid;
}

void check_killed()
{
	pcb_t *current_running = get_current_running();
	spin_lock_acquire(&current_running->lock);
	if (current_running->killed) {
		spin_lock_release(&current_running->lock);
		do_exit();
	}
	spin_lock_release(&current_running->lock);
}

void do_exit(void)
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	printl("pid %d exit\n", current_running->pid);
	spin_lock_acquire(&current_running->lock);
	while (!list_empty(&current_running->wait_list)) {
		do_unblock(list_front(&current_running->wait_list));
	}
	list_del(&current_running->list);
	do_destroy_pthread_lock(current_running->pid);
	current_running->status = TASK_EXITED;
	current_running->cursor_x = 0;
	current_running->cursor_y = 0;
	current_running->kernel_sp =
		current_running->kernel_stack_base + PAGE_SIZE;
	current_running->user_stack_base = 0xf0000f000;
	current_running->user_sp = current_running->user_stack_base + PAGE_SIZE;
	unmapPageDir(current_running->pid);
	current_running->pid = 0;
	current_running->tid = 0;
	current_running->wakeup_time = 0;
	current_running->killed = 0;
	current_running->next_stack_base =
		current_running->user_stack_base + 2 * PAGE_SIZE;
	do_scheduler();
}

int do_waitpid(pid_t pid)
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	int pcbidx = -1;
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		spin_lock_acquire(&pcb[i].lock);
		if (pcb[i].pid == pid) {
			pcbidx = i;
			break;
		}
		spin_lock_release(&pcb[i].lock);
	}
	if (pcbidx == -1)
		return 0;
	spin_lock_acquire(&current_running->lock);
	current_running->status = TASK_BLOCKED;
	do_block(&current_running->list, &pcb[pcbidx].wait_list);
	spin_lock_release(&pcb[pcbidx].lock);
	do_scheduler();
	return pid;
}

void do_process_show()
{
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		spin_lock_acquire(&pcb[i].lock);
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
		spin_lock_release(&pcb[i].lock);
	}
}

int do_taskset(pid_t pid, int mask)
{
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		spin_lock_acquire(&pcb[i].lock);
		if (pid == pcb[i].pid) {
			pcb[i].cpuMask = mask;
			spin_lock_release(&pcb[i].lock);
			return 0;
		}
		spin_lock_release(&pcb[i].lock);
	}
	return 1;
}

pcb_t *pid2pcb(int pid)
{
	int ret = 0;
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		spin_lock_acquire(&pcb[i].lock);
		if (pid == pcb[i].pid) {
			ret = i;
		}
		spin_lock_release(&pcb[i].lock);
	}
	return &pcb[ret];
}

void init_pthread_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
			pcb_t *pcb, void *arg)
{
	// [p3-task1] set user stack for argv
	ptr_t vUserSp = user_stack;
	ptr_t pUserSp = allocPage(pcb->pid, 0, PINNED);
	map_page(vUserSp - PAGE_SIZE, kva2pa((uintptr_t)pUserSp), pcb->pagedir,
		 pcb->pid);
	pcb->user_sp = vUserSp;
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
	pt_regs->regs[1] = (reg_t)(0x10000 + 2);
	pt_regs->regs[10] = (reg_t)arg;
	// When a trap is taken, SPP is set to 0 if the trap originated from user mode, or 1 otherwise.
	pt_regs->sstatus = ((reg_t)SR_SPIE & (reg_t)~SR_SPP) | (reg_t)SR_SUM;
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
	// pt_switchto->regs[0] = (reg_t)entry_point;         // ra
	// [p2-task3]
	// pt_switchto->regs[0] = (reg_t)ret_from_exception;   // ra
	// [p3-task5]
	pt_switchto->regs[0] = (reg_t)forkret; // ra
	pt_switchto->regs[1] = (reg_t)pt_switchto; // sp

	pcb->kernel_sp = (reg_t)pt_switchto;
}

int thread_create(int *tidptr, long entrypoint, void *arg)
{
	pcb_t *current_running = get_current_running();
	int pcbidx = -1;
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		spin_lock_acquire(&pcb[i].lock);
		if (pcb[i].status == TASK_EXITED) {
			pcbidx = i;
			break;
		}
		spin_lock_release(&pcb[i].lock);
	}
	assert(pcbidx != -1);
	pcb[pcbidx].pid = process_id++;
	pcb[pcbidx].pagedir = current_running->pagedir;
	pcb[pcbidx].user_stack_base = current_running->next_stack_base;
	current_running->next_stack_base += 2 * PAGE_SIZE;
	init_pthread_stack(pcb[pcbidx].kernel_stack_base + PAGE_SIZE,
			   pcb[pcbidx].user_stack_base + PAGE_SIZE, entrypoint,
			   pcb + pcbidx, arg);
	pcb[pcbidx].status = TASK_READY;
	pcb[pcbidx].cursor_x = 0;
	pcb[pcbidx].cursor_y = 0;
	pcb[pcbidx].tid = 0;
	pcb[pcbidx].wakeup_time = 0;
	pcb[pcbidx].cpuMask = current_running->cpuMask;
	spin_lock_acquire(&ready_queue_lock);
	list_push(&ready_queue, &pcb[pcbidx].list);
	spin_lock_release(&ready_queue_lock);
	spin_lock_release(&pcb[pcbidx].lock);
	*tidptr = pcb[pcbidx].pid;
	return 0;
}

void fork_copy_pgtable(PTE *dest, PTE *src, int pid)
{
	for (uint64_t i = 0; i < 512; ++i) {
		if (src[i] == 0)
			continue;
		PTE *secondPgdir = (PTE *)pa2kva(get_pa(src[i]));
		for (uint64_t j = 0; j < 512; ++j) {
			if (secondPgdir[j] == 0)
				continue;
			PTE *thirdPgdir = (PTE *)pa2kva(get_pa(secondPgdir[j]));
			for (uint64_t k = 0; k < 512; ++k) {
				uint64_t va = ((i << (PPN_BITS + PPN_BITS)) |
					       (j << (PPN_BITS)) | k)
					      << (NORMAL_PAGE_SHIFT);
				if (va > 0x0000003fffffffff)
					break;
				if (thirdPgdir[k] == 0)
					continue;
				PTE *entry = &thirdPgdir[k];
				map_page(va, get_pa(*entry), dest, pid);
				PTE *new_entry = getEntry(dest, va);
				long bits = get_attribute(*entry,
							  1023 ^ _PAGE_WRITE) |
					    _PAGE_SOFT_FORK;
				set_attribute(new_entry, bits);
				set_attribute(entry, bits);
				// long entry_new_bits =
				// 	get_attribute(*entry, 1023);
				// long new_entry_new_bits =
				// 	get_attribute(*new_entry, 1023);
				uint64_t kva = pa2kva(get_pa(*entry));
				int idx = addr2idx(kva);
				++pgcb[idx].cnt;
				printl("page %d add cnt, cnt = %d\n", idx,
				       pgcb[idx].cnt);
			}
		}
	}
}

int do_fork()
{
	pcb_t *current_running = get_current_running();
	int pcbidx = -1;
	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		spin_lock_acquire(&pcb[i].lock);
		if (pcb[i].status == TASK_EXITED) {
			pcbidx = i;
			break;
		}
		spin_lock_release(&pcb[i].lock);
	}
	assert(pcbidx != -1);
	pcb[pcbidx].status = TASK_READY;
	int ch_pid = process_id++;
	pcb[pcbidx].kernel_sp = current_running->kernel_sp +
				pcb[pcbidx].kernel_stack_base -
				current_running->kernel_stack_base;
	pcb[pcbidx].user_sp = current_running->user_sp;
	pcb[pcbidx].user_stack_base = current_running->user_stack_base;
	pcb[pcbidx].cpuMask = current_running->cpuMask;
	pcb[pcbidx].pid = ch_pid;
	pcb[pcbidx].cursor_x = current_running->cursor_x;
	pcb[pcbidx].cursor_y = current_running->cursor_y;
	pcb[pcbidx].wakeup_time = current_running->wakeup_time;
	pcb[pcbidx].killed = current_running->killed;
	pcb[pcbidx].pagedir = initPgtable(pcb[pcbidx].pid);
	pcb[pcbidx].next_stack_base = current_running->next_stack_base;
	memcpy((uint8_t *)pcb[pcbidx].kernel_stack_base,
	       (uint8_t *)current_running->kernel_stack_base, PAGE_SIZE);
	regs_context_t *pt_regs = (regs_context_t *)pcb[pcbidx].kernel_sp;
	pt_regs->regs[2] = (reg_t)pcb[pcbidx].user_sp;
	pt_regs->regs[4] = (reg_t)&pcb[pcbidx];
	pt_regs->regs[10] = (reg_t)0;
	pt_regs->sepc += 4;
	switchto_context_t *pt_switchto =
		(switchto_context_t *)((ptr_t)pt_regs -
				       sizeof(switchto_context_t));
	for (int i = 0; i < 14; ++i)
		pt_switchto->regs[i] = (reg_t)0;
	pt_switchto->regs[0] = (reg_t)forkret; // ra
	pt_switchto->regs[1] = (reg_t)pt_switchto; // sp

	pcb[pcbidx].kernel_sp = (reg_t)pt_switchto;

	fork_copy_pgtable(pcb[pcbidx].pagedir, current_running->pagedir,
			  pcb[pcbidx].pid);

	spin_lock_acquire(&ready_queue_lock);
	list_push(&ready_queue, &pcb[pcbidx].list);
	spin_lock_release(&ready_queue_lock);
	printl("ready_queue: ");
	list_print(&ready_queue);
	spin_lock_release(&pcb[pcbidx].lock);

	local_flush_tlb_all();
	local_flush_icache_all();

	return ch_pid;
}