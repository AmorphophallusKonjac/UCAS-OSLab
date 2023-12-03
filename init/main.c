#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/smp.h>
#include <os/irq.h>
#include <os/page.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <pgtable.h>

extern void ret_from_exception();
extern void clear_SIP();
#define VERSION_BUF 50
#define OS_SIZE_LOC 0xffffffc0502001fclu
#define STACK_PAGE_NUM 1

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

// [p1-Task3] Task num
uint16_t task_num;

static int bss_check(void)
{
	for (int i = 0; i < VERSION_BUF; ++i) {
		if (buf[i] != 0) {
			return 0;
		}
	}
	return 1;
}

static void init_jmptab(void)
{
	volatile long (*(*jmptab))() =
		(volatile long (*(*))())KERNEL_JMPTAB_BASE;

	jmptab[CONSOLE_PUTSTR] = (long (*)())port_write;
	jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
	jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
	jmptab[SD_READ] = (long (*)())sd_read;
	jmptab[SD_WRITE] = (long (*)())sd_write;
	jmptab[QEMU_LOGGING] = (long (*)())qemu_logging;
	jmptab[SET_TIMER] = (long (*)())set_timer;
	jmptab[READ_FDT] = (long (*)())read_fdt;
	jmptab[MOVE_CURSOR] = (long (*)())screen_move_cursor;
	jmptab[PRINT] = (long (*)())printk;
	jmptab[YIELD] = (long (*)())yield;
	jmptab[MUTEX_INIT] = (long (*)())do_mutex_lock_init;
	jmptab[MUTEX_ACQ] = (long (*)())do_mutex_lock_acquire;
	jmptab[MUTEX_RELEASE] = (long (*)())do_mutex_lock_release;

	// TODO: [p2-task1] (S-core) initialize system call table.
	jmptab[WRITE] = (long (*)())screen_write;
	jmptab[REFLUSH] = (long (*)())screen_reflush;
}

static void init_task_info(void)
{
	// TODO: [p1-task4] Init 'tasks' array via reading app-info sector
	// NOTE: You need to get some related arguments from bootblock first
	uint64_t taskinfo_size = *(uint32_t *)(OS_SIZE_LOC - 0x8);
	uint64_t taskinfo_offset = *(uint32_t *)(OS_SIZE_LOC - 0xC);
	taskinfo_offset = taskinfo_offset % (uint64_t)(0x200);
	task_num = *(uint16_t *)(OS_SIZE_LOC + 0x2);
	memcpy((uint8_t *)tasks,
	       (uint8_t *)(0xffffffc052000000lu + taskinfo_offset),
	       taskinfo_size);
}

/************************************************************/

static void init_pcb(void)
{
	/* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
	char needed_task_name[][32] = { "shell" };
	pid0_pcb[0].kernel_sp = allocPage(0, 0, PINNED) + PAGE_SIZE;
	pid0_pcb[0].user_sp = pid0_pcb[0].kernel_sp;
	pid0_pcb[0].pagedir = initPgtable(0);

	pid0_pcb[1].kernel_sp = allocPage(0, 0, PINNED) + PAGE_SIZE;
	pid0_pcb[1].user_sp = pid0_pcb[1].kernel_sp;
	pid0_pcb[1].pagedir = initPgtable(0);

	for (int i = 0; i < NUM_MAX_TASK; ++i) {
		pcb[i].kernel_stack_base = allocPage(0, 0, PINNED);
		pcb[i].user_stack_base = 0xf0000f000;
		pcb[i].status = TASK_EXITED;
		pcb[i].list.next = &pcb[i].list;
		pcb[i].list.prev = &pcb[i].list;
		pcb[i].wait_list.next = &pcb[i].wait_list;
		pcb[i].wait_list.prev = &pcb[i].wait_list;
		spin_lock_init(&pcb[i].lock);
		pcb[i].pagedir = NULL;
		pcb[i].next_stack_base = pcb[i].user_stack_base + 2 * PAGE_SIZE;
	}

	for (int i = 0; i < sizeof(needed_task_name) / 32; i++) {
		pcb[i].pid = process_id++;
		pcb[i].status = TASK_READY;
		pcb[i].cursor_x = 0;
		pcb[i].cursor_y = 0;
		pcb[i].wakeup_time = 0;
		pcb[i].cpuMask = 3;
		pcb[i].pagedir = initPgtable(pcb[i].pid);
		init_pcb_stack(pcb[i].kernel_stack_base + PAGE_SIZE,
			       pcb[i].user_stack_base + PAGE_SIZE,
			       getEntrypoint(needed_task_name[i]), pcb + i, 0,
			       0);
		list_push(&ready_queue, &pcb[i].list);
		pcb[i].pagedir = initPgtable(pcb[i].pid);
		from_name_load_task_img(needed_task_name[i], &pcb[i]);
	}

	/* TODO: [p2-task1] remember to initialize 'current_running' */
	cpu[0].current_running = &pid0_pcb[0];
	cpu[0].pid = 0;
	asm volatile("mv tp, %0;"
		     :
		     : "r"(cpu[0].current_running)); // set tp = current_running
}

static void init_syscall(void)
{
	// TODO: [p2-task3] initialize system call table.
	syscall[SYSCALL_EXEC] = (long (*)())do_exec;
	syscall[SYSCALL_EXIT] = (long (*)())do_exit;
	syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
	syscall[SYSCALL_KILL] = (long (*)())do_kill;
	syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
	syscall[SYSCALL_PS] = (long (*)())do_process_show;
	syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
	syscall[SYSCALL_YIELD] = (long (*)())yield;

	syscall[SYSCALL_WRITE] = (long (*)())screen_write;
	syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
	syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
	syscall[SYSCALL_CLEAR] = (long (*)())screen_clear;
	syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
	syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
	syscall[SYSCALL_SEMA_INIT] = (long (*)())do_semaphore_init;
	syscall[SYSCALL_SEMA_UP] = (long (*)())do_semaphore_up;
	syscall[SYSCALL_SEMA_DOWN] = (long (*)())do_semaphore_down;
	syscall[SYSCALL_SEMA_DESTROY] = (long (*)())do_semaphore_destroy;
	syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
	syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
	syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;

	syscall[SYSCALL_BARR_INIT] = (long (*)())do_barrier_init;
	syscall[SYSCALL_BARR_WAIT] = (long (*)())do_barrier_wait;
	syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;
	syscall[SYSCALL_COND_INIT] = (long (*)())do_condition_init;
	syscall[SYSCALL_COND_WAIT] = (long (*)())do_condition_wait;
	syscall[SYSCALL_COND_SIGNAL] = (long (*)())do_condition_signal;
	syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
	syscall[SYSCALL_COND_DESTROY] = (long (*)())do_condition_destroy;
	syscall[SYSCALL_MBOX_OPEN] = (long (*)())do_mbox_open;
	syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
	syscall[SYSCALL_MBOX_SEND] = (long (*)())do_mbox_send;
	syscall[SYSCALL_MBOX_RECV] = (long (*)())do_mbox_recv;
	syscall[SYSCALL_SHM_GET] = (long (*)())shm_page_get;
	syscall[SYSCALL_SHM_DT] = (long (*)())shm_page_dt;

	syscall[SYSCALL_BIOS_LOGGING] = (long (*)())bios_logging;
	syscall[SYSCALL_THREAD_CREATE] = (long (*)())thread_create;
	syscall[SYSCALL_THREAD_YIELD] = (long (*)())yield;
	syscall[SYSCALL_READCH] = (long (*)())bios_getchar;
	syscall[SYSCALL_BACKSPACE] = (long (*)())screen_backspace;
	syscall[SYSCALL_SCREEN_CLEAR] = (long (*)())screen_clear;
	syscall[SYSCALL_HIDDEN_CURSOR] = (long (*)())screen_hidden_cursor;
	syscall[SYSCALL_SHOW_CURSOR] = (long (*)())screen_show_cursor;
	syscall[SYSCALL_TASKSET] = (long (*)())do_taskset;
}
/************************************************************/

int main(void)
{
	if (get_current_cpu_id() == 0) {
		// Check whether .bss section is set to zero
		int check = bss_check();

		// Init jump table provided by kernel and bios(ΦωΦ)
		init_jmptab();

		// Init task information (〃'▽'〃)
		init_task_info();

		// Output 'Hello OS!', bss check result and OS version
		char output_str[] = "bss check: _ version: _\n\r";
		char output_val[2] = { 0 };
		int i, output_val_pos = 0;

		output_val[0] = check ? 't' : 'f';
		output_val[1] = version + '0';
		for (i = 0; i < sizeof(output_str); ++i) {
			buf[i] = output_str[i];
			if (buf[i] == '_') {
				buf[i] = output_val[output_val_pos++];
			}
		}

		bios_putstr("Hello OS!\n\r");
		bios_putstr(buf);

		// Init Physical memory allocator
		initkmem();
		bios_putstr("[INIT] Memory initialization succeeded.\n\r");

		// Init Process Control Blocks |•'-'•) ✧
		init_pcb();
		printk("> [INIT] PCB initialization succeeded.\n");

		// Read CPU frequency (｡•ᴗ-)_
		time_base = bios_read_fdt(TIMEBASE);

		// Init lock mechanism o(´^｀)o
		init_locks();
		printk("> [INIT] Lock mechanism initialization succeeded.\n");

		// Init semaphore mechanism o(´^｀)o
		init_semaphores();
		printk("> [INIT] semaphores mechanism initialization succeeded.\n");

		// Init barrier mechanism o(´^｀)o
		init_barriers();
		printk("> [INIT] barriers mechanism initialization succeeded.\n");

		// Init condition mechanism o(´^｀)o
		init_conditions();
		printk("> [INIT] condition mechanism initialization succeeded.\n");

		// Init mailbox mechanism o(´^｀)o
		init_mbox();
		printk("> [INIT] mailbox mechanism initialization succeeded.\n");

		// Init interrupt (^_^)
		init_exception();
		printk("> [INIT] Interrupt processing initialization succeeded.\n");

		// Init system call table (0_0)
		init_syscall();
		printk("> [INIT] System call initialized successfully.\n");

		// Init screen (QAQ)
		init_screen();
		printk("> [INIT] SCREEN initialization succeeded.\n");

		printk("> [INIT] CPU0 starting.\n");

		smp_init();
		wakeup_other_hart();
		clear_SIP();

	} else {
		unmapBoot();
		// lock_kernel();
		cpu[1].current_running = &pid0_pcb[1];
		cpu[1].pid = 0;
		asm volatile(
			"mv tp, %0;"
			:
			: "r"(cpu[1].current_running)); // set tp = current_running
		// unlock_kernel();
		setup_exception();
		screen_move_cursor(0, 2);
		printk("> [INIT] CPU1 starting.\n");
	}

	// [p2-task4] set the first time interupt
	bios_set_timer(get_ticks() + TIMER_INTERVAL);

	// Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
	while (1) {
		// If you do non-preemptive scheduling, it's used to surrender control
		// do_scheduler();

		// If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
		enable_preempt();
		asm volatile("wfi");
	}

	return 0;
}
