#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
semaphore_t sema[SEMAPHORE_NUM];
condition_t cond[CONDITION_NUM];
barrier_t bar[BARRIER_NUM];

void init_locks(void)
{
	/* TODO: [p2-task2] initialize mlocks */
	for (int i = 0; i < LOCK_NUM; ++i) {
		mutex_destroy(&mlocks[i]);
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

void mutex_destroy(mutex_lock_t *mutex)
{
	list_destroy(&mutex->block_queue);
	mutex->key = -1;
	mutex->lock.status = UNLOCKED;
	mutex->pid = 0;
}

void mutex_lock_acquire(mutex_lock_t *mutex)
{
	while (atomic_swap(LOCKED, (ptr_t)&mutex->lock.status) == LOCKED) {
		do_block(&current_running->list, &mutex->block_queue);
		do_scheduler();
	}
	mutex->pid = current_running->pid;
}

void mutex_lock_release(mutex_lock_t *mutex)
{
	mutex->pid = 0;
	while (!list_empty(&mutex->block_queue)) {
		do_unblock(list_front(&mutex->block_queue));
	}
	atomic_swap(UNLOCKED, (ptr_t)&mutex->lock.status);
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
	mutex_lock_acquire(&mlocks[mlock_idx]);
}

void do_mutex_lock_release(int mlock_idx)
{
	/* TODO: [p2-task2] release mutex lock */
	mutex_lock_release(&mlocks[mlock_idx]);
}

void do_destroy_pthread_lock(int pid)
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
		semaphore_destroy(&sema[i]);
	}
}

int do_semaphore_init(int key, int init)
{
	int ret = -1;
	for (int i = 0; i < SEMAPHORE_NUM; ++i) {
		mutex_lock_acquire(&sema[i].mutex);
		if (sema[i].key == -1) {
			ret = i;
			sema[i].key = key;
			sema[i].value = init;
			mutex_lock_release(&sema[i].mutex);
			break;
		}
		mutex_lock_release(&sema[i].mutex);
	}
	return ret;
}

void do_semaphore_up(int sema_idx)
{
	mutex_lock_acquire(&sema[sema_idx].mutex);
	if (!list_empty(&sema[sema_idx].wait_list)) {
		do_unblock(list_front(&sema[sema_idx].wait_list));
	} else {
		++sema[sema_idx].value;
	}
	mutex_lock_release(&sema[sema_idx].mutex);
}

void do_semaphore_down(int sema_idx)
{
	mutex_lock_acquire(&sema[sema_idx].mutex);
	if (sema[sema_idx].value == 0) {
		do_block(&current_running->list, &sema[sema_idx].wait_list);
		mutex_lock_release(&sema[sema_idx].mutex);
		do_scheduler();
	} else {
		--sema[sema_idx].value;
		mutex_lock_release(&sema[sema_idx].mutex);
	}
}

void semaphore_destroy(semaphore_t *semaphore)
{
	semaphore->key = -1;
	semaphore->value = 0;
	list_destroy(&semaphore->wait_list);
	mutex_destroy(&semaphore->mutex);
	semaphore->pid = 0;
}

void do_semaphore_destroy(int sema_idx)
{
	semaphore_destroy(&sema[sema_idx]);
}

void init_conditions(void)
{
	for (int i = 0; i < CONDITION_NUM; ++i) {
		condition_destroy(&cond[i]);
	}
}

int do_condition_init(int key)
{
	int ret = -1;
	for (int i = 0; i < CONDITION_NUM; ++i) {
		mutex_lock_acquire(&cond[i].mutex);
		if (cond[i].key == -1) {
			ret = i;
			cond[i].key = key;
			mutex_lock_release(&cond[i].mutex);
			break;
		}
		mutex_lock_release(&cond[i].mutex);
	}
	return ret;
}

void do_condition_wait(int cond_idx, int mutex_idx)
{
	do_mutex_lock_release(mutex_idx);
	mutex_lock_acquire(&cond[cond_idx].mutex);
	do_block(&current_running->list, &cond[cond_idx].wait_list);
	mutex_lock_release(&cond[cond_idx].mutex);
	do_scheduler();
}

void do_condition_signal(int cond_idx)
{
	mutex_lock_acquire(&cond[cond_idx].mutex);
	if (!list_empty(&cond[cond_idx].wait_list))
		do_unblock(list_front(&cond[cond_idx].wait_list));
	mutex_lock_release(&cond[cond_idx].mutex);
}

void do_condition_broadcast(int cond_idx)
{
	mutex_lock_acquire(&cond[cond_idx].mutex);
	while (!list_empty(&cond[cond_idx].wait_list)) {
		do_unblock(list_front(&cond[cond_idx].wait_list));
	}
	mutex_lock_release(&cond[cond_idx].mutex);
}

void condition_destroy(condition_t *condition)
{
	condition->key = -1;
	mutex_destroy(&condition->mutex);
	list_destroy(&condition->wait_list);
	condition->pid = 0;
}

void do_condition_destroy(int cond_idx)
{
	condition_destroy(&cond[cond_idx]);
}

void init_barriers(void)
{
	for (int i = 0; i < BARRIER_NUM; ++i) {
		barrier_destroy(&bar[i]);
	}
}

int do_barrier_init(int key, int goal)
{
	int ret = -1;
	for (int i = 0; i < BARRIER_NUM; ++i) {
		mutex_lock_acquire(&bar[i].mutex);
		if (bar[i].key == -1) {
			ret = i;
			bar[i].key = key;
			bar[i].goal = goal;
			mutex_lock_release(&bar[i].mutex);
			break;
		}
		mutex_lock_release(&bar[i].mutex);
	}
	return ret;
}

void do_barrier_wait(int bar_idx)
{
	mutex_lock_acquire(&bar[bar_idx].mutex);
	++bar[bar_idx].cnt;
	if (bar[bar_idx].cnt >= bar[bar_idx].goal) {
		bar[bar_idx].cnt = 0;
		while (!list_empty(&bar[bar_idx].wait_list)) {
			do_unblock(list_front(&bar[bar_idx].wait_list));
		}
		mutex_lock_release(&bar[bar_idx].mutex);
	} else {
		do_block(&current_running->list, &bar[bar_idx].wait_list);
		mutex_lock_release(&bar[bar_idx].mutex);
		do_scheduler();
	}
}

void barrier_destroy(barrier_t *barrier)
{
	barrier->cnt = 0;
	barrier->goal = 0;
	barrier->key = -1;
	mutex_destroy(&barrier->mutex);
	list_destroy(&barrier->wait_list);
}

void do_barrier_destroy(int bar_idx)
{
	barrier_destroy(&bar[bar_idx]);
}
