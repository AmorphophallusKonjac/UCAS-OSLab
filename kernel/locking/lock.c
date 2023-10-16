#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

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
	mlocks[mlock_idx].lock.status = LOCKED;
}

void do_mutex_lock_release(int mlock_idx)
{
	/* TODO: [p2-task2] release mutex lock */
	mlocks[mlock_idx].lock.status = UNLOCKED;
	if (list_empty(&mlocks[mlock_idx].block_queue))
		return;
	do_unblock(list_front(&mlocks[mlock_idx].block_queue));
}
