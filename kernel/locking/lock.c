#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
semaphore_t sems[SEMAPHORE_NUM];

void init_locks(void)
{
	/* TODO: [p2-task2] initialize mlocks */
	for (int i = 0; i < LOCK_NUM; ++i) {
		mlocks[i].block_queue.next = &mlocks[i].block_queue;
		mlocks[i].block_queue.prev = &mlocks[i].block_queue;
		mlocks[i].key = -1;
		mlocks[i].lock.status = UNLOCKED;
	}
}

void spin_lock_init(spin_lock_t *lock)
{
	/* TODO: [p2-task2] initialize spin lock */
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
	/* TODO: [p2-task2] try to acquire spin lock */
	return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
	/* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
	/* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)
{
	/* TODO: [p2-task2] initialize mutex lock */
	int ret = -1;
	for (int i = 0; i < LOCK_NUM; ++i) {
		if (mlocks[i].key == key) {
			ret = i;
			break;
		}
	}
	if (ret != -1)
		return ret;
	for (int i = 0; i < LOCK_NUM; ++i) {
		if (mlocks[i].key == -1) {
			ret = i;
			break;
		}
	}
	mlocks[ret].block_queue.next = &mlocks[ret].block_queue;
	mlocks[ret].block_queue.prev = &mlocks[ret].block_queue;
	mlocks[ret].key = key;
	mlocks[ret].lock.status = UNLOCKED;
	mlocks[ret].pid = 0;
	return ret;
}

void do_mutex_lock_acquire(int mlock_idx)
{
	/* TODO: [p2-task2] acquire mutex lock */
	while (mlocks[mlock_idx].lock.status == LOCKED) {
		if (current_running->status != TASK_BLOCKED) {
			current_running->status = TASK_BLOCKED;
			do_block(&current_running->list,
				 &mlocks[mlock_idx].block_queue);
		}
		do_scheduler();
	}
	mlocks[mlock_idx].pid = current_running->pid;
	mlocks[mlock_idx].lock.status = LOCKED;
}

void do_mutex_lock_release(int mlock_idx)
{
	/* TODO: [p2-task2] release mutex lock */
	mlocks[mlock_idx].lock.status = UNLOCKED;
	mlocks[mlock_idx].pid = 0;
	if (list_empty(&mlocks[mlock_idx].block_queue))
		return;
	while (!list_empty(&mlocks[mlock_idx].block_queue)) {
		do_unblock(list_front(&mlocks[mlock_idx].block_queue));
	}
}

void do_pid_lock_release(int pid)
{
	for (int i = 0; i < LOCK_NUM; ++i) {
		if (mlocks[i].pid == pid) {
			do_mutex_lock_release(i);
		}
	}
}

void init_semaphores(void)
{
	for (int i = 0; i < SEMAPHORE_NUM; ++i) {
		sems[i].key = -1;
		sems[i].value = 0;
		sems[i].mutex.block_queue.next = &sems[i].mutex.block_queue;
		sems[i].mutex.block_queue.prev = &sems[i].mutex.block_queue;
		sems[i].mutex.lock.status = UNLOCKED;
		sems[i].wait_list.next = &sems[i].wait_list;
		sems[i].wait_list.prev = &sems[i].wait_list;
	}
}

void do_semaphore_mutex_lock_acquire(int sema_idx)
{
	while (atomic_swap(LOCKED, (ptr_t)&sems[sema_idx].mutex.lock.status) ==
	       LOCKED) {
		current_running->status = TASK_BLOCKED;
		do_block(&current_running->list,
			 &sems[sema_idx].mutex.block_queue);
		do_scheduler();
	}
}

void do_semaphore_mutex_lock_release(int sema_idx)
{
	atomic_swap(UNLOCKED, (ptr_t)&sems[sema_idx].mutex.lock.status);
	while (!list_empty(&sems[sema_idx].mutex.block_queue)) {
		do_unblock(list_front(&sems[sema_idx].mutex.block_queue));
	}
}

int do_semaphore_init(int key, int init)
{
	int ret = -1;
	for (int i = 0; i < SEMAPHORE_NUM; ++i) {
		do_semaphore_mutex_lock_acquire(i);
		if (sems[i].key == -1) {
			ret = i;
			sems[i].key = key;
			sems[i].value = init;
			do_semaphore_mutex_lock_release(i);
			break;
		}
	}
	return ret;
}

void do_semaphore_up(int sema_idx)
{
	do_semaphore_mutex_lock_acquire(sema_idx);
	if (!list_empty(&sems[sema_idx].wait_list)) {
		do_unblock(list_front(&sems[sema_idx].wait_list));
	} else {
		++sems[sema_idx].value;
	}
	do_semaphore_mutex_lock_release(sema_idx);
}

void do_semaphore_down(int sema_idx)
{
	do_semaphore_mutex_lock_acquire(sema_idx);
	if (sems[sema_idx].value == 0) {
		do_semaphore_mutex_lock_release(sema_idx);
		current_running->status = TASK_BLOCKED;
		do_block(&current_running->list, &sems[sema_idx].wait_list);
		do_scheduler();
	} else {
		--sems[sema_idx].value;
		do_semaphore_mutex_lock_release(sema_idx);
	}
}

void do_semaphore_destroy(int sema_idx)
{
	sems[sema_idx].key = -1;
	sems[sema_idx].value = 0;
	sems[sema_idx].mutex.block_queue.next =
		&sems[sema_idx].mutex.block_queue;
	sems[sema_idx].mutex.block_queue.prev =
		&sems[sema_idx].mutex.block_queue;
	sems[sema_idx].mutex.lock.status = UNLOCKED;
	sems[sema_idx].wait_list.next = &sems[sema_idx].wait_list;
	sems[sema_idx].wait_list.prev = &sems[sema_idx].wait_list;
}
