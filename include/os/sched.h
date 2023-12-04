/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS Author :
 * Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Process scheduling related content, such as: scheduler, process
 * blocking, process wakeup, process creation, process kill, etc.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <os/list.h>
#include <os/lock.h>
#include <type.h>
#include <pgtable.h>

#define NUM_MAX_TASK 16
#define NUM_MAX_CPU 2

/* used to save register infomation */
typedef struct regs_context {
	/* Saved main processor registers.*/
	reg_t regs[32];

	/* Saved special registers. */
	reg_t sstatus;
	reg_t sepc;
	reg_t sbadaddr;
	reg_t scause;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context {
	/* Callee saved registers.*/
	reg_t regs[14];
} switchto_context_t;

typedef enum {
	TASK_BLOCKED,
	TASK_RUNNING,
	TASK_READY,
	TASK_EXITED,
	TASK_IDLE
} task_status_t;

/* Process Control Block */
typedef struct pcb {
	/* register context */
	// NOTE: this order must be preserved, which is defined in regs.h!!
	reg_t kernel_sp;
	reg_t user_sp;
	ptr_t kernel_stack_base;
	ptr_t user_stack_base;

	/* previous, next pointer */
	list_node_t list;
	list_head wait_list;

	uint64_t cpuID;
	int cpuMask;

	/* process id */
	pid_t pid;
	/* */
	tid_t tid;

	/* BLOCK | READY | RUNNING */
	task_status_t status;

	/* cursor position */
	int cursor_x;
	int cursor_y;

	/* time(seconds) to wake up sleeping PCB */
	uint64_t wakeup_time;

	/* lock to protect pcb */
	spin_lock_t lock;

	/* the pcb switch from */
	struct pcb *switch_from;

	/* set 1 if killed */
	int killed;

	/* page dir */
	PTE *pagedir;

	ptr_t next_stack_base;

} pcb_t, tcb_t;

/* CPU */
typedef struct cpu {
	pid_t pid;
	pcb_t *volatile current_running;
} cpu_t;

/* ready queue to run */
extern list_head ready_queue;
extern spin_lock_t ready_queue_lock;

/* sleep queue to be blocked in */
extern list_head sleep_queue;
extern spin_lock_t sleep_queue_lock;

extern cpu_t cpu[NUM_MAX_CPU];

/* current running task PCB */
// extern pcb_t *volatile current_running;
extern pid_t process_id;
extern tid_t thread_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern tcb_t tcb[NUM_MAX_TASK];
extern pcb_t pid0_pcb[2];
extern const ptr_t pid0_stack;

extern void switch_to(pcb_t *prev, pcb_t *next);
void update_current_running();
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *, list_head *queue);
void do_unblock(list_node_t *);

// [p2-task1]
#define NODE2PCB(nodeptr) ((pcb_t *)((void *)(nodeptr)-32))

extern int do_taskset(pid_t pid, int mask);

// [p3-task5]
/* new fake return entrypoint */
extern void forkret();
void release_lock(void);
void yield(void);
void check_killed();
pcb_t *pid2pcb(int pid);
void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
		    pcb_t *pcb, int argc, char *argv[]);
int thread_create(int *tidptr, long func, void *arg);
int do_fork();
/************************************************************/
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1,
		     uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif
extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_getpid();
/************************************************************/

#endif
