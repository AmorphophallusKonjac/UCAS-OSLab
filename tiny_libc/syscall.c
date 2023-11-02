#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>
#include <stdarg.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
			   long arg3, long arg4)
{
	/* TODO: [p2-task3] implement invoke_syscall via inline assembly */
	long ret = 0;
	asm volatile("mv a7, %1;\
                  mv a0, %2;\
                  mv a1, %3;\
                  mv a2, %4;\
                  mv a3, %5;\
                  mv a4, %6;\
                  ecall;\
                  mv %0, a0;"
		     : "=r"(ret)
		     : "r"(sysno), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3),
		       "r"(arg4));

	return ret;
}

void sys_yield(void)
{
	/* TODO: [p2-task1] call call_jmptab to implement sys_yield */
	// call_jmptab(YIELD, 0, 0, 0, 0, 0);
	/* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
	invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y)
{
	/* TODO: [p2-task1] call call_jmptab to implement sys_move_cursor */
	// call_jmptab(MOVE_CURSOR, (long) x, (long) y, 0, 0, 0);
	/* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
	invoke_syscall(SYSCALL_CURSOR, x, y, IGNORE, IGNORE, IGNORE);
}

void sys_write(char *buff)
{
	/* TODO: [p2-task1] call call_jmptab to implement sys_write */
	// call_jmptab(WRITE, (long) buff, 0, 0, 0, 0);
	/* TODO: [p2-task3] call invoke_syscall to implement sys_write */
	invoke_syscall(SYSCALL_WRITE, (long)buff, IGNORE, IGNORE, IGNORE,
		       IGNORE);
}

void sys_reflush(void)
{
	/* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
	// call_jmptab(REFLUSH, 0, 0, 0, 0, 0);
	/* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
	invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mutex_init(int key)
{
	/* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
	// return call_jmptab(MUTEX_INIT, key, 0, 0, 0, 0);
	/* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
	return invoke_syscall(SYSCALL_LOCK_INIT, key, IGNORE, IGNORE, IGNORE,
			      IGNORE);
}

void sys_mutex_acquire(int mutex_idx)
{
	/* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
	// call_jmptab(MUTEX_ACQ, mutex_idx, 0, 0, 0, 0);
	/* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
	invoke_syscall(SYSCALL_LOCK_ACQ, mutex_idx, IGNORE, IGNORE, IGNORE,
		       IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
	/* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
	// call_jmptab(MUTEX_RELEASE, mutex_idx, 0, 0, 0, 0);
	/* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
	invoke_syscall(SYSCALL_LOCK_RELEASE, mutex_idx, IGNORE, IGNORE, IGNORE,
		       IGNORE);
}

long sys_get_timebase(void)
{
	/* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
	return invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE,
			      IGNORE, IGNORE);
}

long sys_get_tick(void)
{
	/* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
	return invoke_syscall(SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE, IGNORE,
			      IGNORE);
}

void sys_sleep(uint32_t time)
{
	/* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
	invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_bios_logging(char *buff)
{
	invoke_syscall(SYSCALL_BIOS_LOGGING, (long)buff, IGNORE, IGNORE, IGNORE,
		       IGNORE);
}

int sys_thread_create(int *tidptr, long func, void *arg)
{
	return invoke_syscall(SYSCALL_THREAD_CREATE, (long)tidptr, func,
			      (long)arg, IGNORE, IGNORE);
}

void sys_thread_yield(void)
{
	invoke_syscall(SYSCALL_THREAD_YIELD, IGNORE, IGNORE, IGNORE, IGNORE,
		       IGNORE);
}

void sys_backspace(void)
{
	invoke_syscall(SYSCALL_BACKSPACE, IGNORE, IGNORE, IGNORE, IGNORE,
		       IGNORE);
}

/************************************************************/
#ifdef S_CORE
pid_t sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
	/* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}
#else
pid_t sys_exec(char *name, int argc, char **argv)
{
	/* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
	invoke_syscall(SYSCALL_EXEC, (long)name, argc, (long)argv, IGNORE,
		       IGNORE);
}
#endif

void sys_exit(void)
{
	/* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
	invoke_syscall(SYSCALL_EXIT, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_kill(pid_t pid)
{
	/* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
	invoke_syscall(SYSCALL_KILL, (long)pid, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_waitpid(pid_t pid)
{
	/* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
	invoke_syscall(SYSCALL_WAITPID, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_ps(void)
{
	/* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
	invoke_syscall(SYSCALL_PS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

pid_t sys_getpid()
{
	/* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
	invoke_syscall(SYSCALL_GETPID, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_getchar(void)
{
	/* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
	invoke_syscall(SYSCALL_READCH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_barrier_init(int key, int goal)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
}

void sys_barrier_wait(int bar_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
}

void sys_barrier_destroy(int bar_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
}

int sys_condition_init(int key)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
}

void sys_condition_signal(int cond_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
}

void sys_condition_broadcast(int cond_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
}

void sys_condition_destroy(int cond_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
}

int sys_semaphore_init(int key, int init)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_init */
}

void sys_semaphore_up(int sema_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_up */
}

void sys_semaphore_down(int sema_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_down */
}

void sys_semaphore_destroy(int sema_idx)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_destroy */
}

int sys_mbox_open(char *name)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
}

void sys_mbox_close(int mbox_id)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
	/* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
}
/************************************************************/