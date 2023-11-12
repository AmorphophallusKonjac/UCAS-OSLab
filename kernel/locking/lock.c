#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <os/smp.h>
#include <printk.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
spin_lock_t slocks[LOCK_NUM];
semaphore_t sema[SEMAPHORE_NUM];
condition_t cond[CONDITION_NUM];
barrier_t bar[BARRIER_NUM];
mailbox_t mbox[MBOX_NUM];

void init_locks(void)
{
	/* TODO: [p2-task2] initialize mlocks */
	for (int i = 0; i < LOCK_NUM; ++i) {
		mutex_init(&mlocks[i]);
		spin_lock_init(&slocks[i]);
	}
}

void spin_lock_init(spin_lock_t *lock)
{
	/* TODO: [p2-task2] initialize spin lock */
	atomic_swap(UNLOCKED, (ptr_t)&lock->status);
	lock->pid = 0;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
	/* TODO: [p2-task2] try to acquire spin lock */
	return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
	/* TODO: [p2-task2] acquire spin lock */
	while (atomic_swap(LOCKED, (ptr_t)&lock->status) == LOCKED)
		;
	lock->pid = get_current_running()->pid;
	// printl("cpu %d owns kernel lock\n\n", get_current_cpu_id());
}

void spin_lock_release(spin_lock_t *lock)
{
	/* TODO: [p2-task2] release spin lock */
	atomic_swap(UNLOCKED, (ptr_t)&lock->status);
	lock->pid = 0;
}

void mutex_init(mutex_lock_t *mutex)
{
	list_destroy(&mutex->block_queue);
	mutex_destroy(mutex);
}

void mutex_destroy(mutex_lock_t *mutex)
{
	mutex->key = -1;
	mutex_lock_release(mutex);
}

void mutex_lock_acquire(mutex_lock_t *mutex)
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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
		spin_lock_acquire(&slocks[i]);
		if (mlocks[i].key == key) {
			ret = i;
			spin_lock_release(&slocks[i]);
			break;
		}
		spin_lock_release(&slocks[i]);
	}
	if (ret != -1)
		return ret;
	for (int i = 0; i < LOCK_NUM; ++i) {
		spin_lock_acquire(&slocks[i]);
		if (mlocks[i].key == -1) {
			ret = i;
			mutex_destroy(&mlocks[i]);
			mlocks[i].key = key;
			spin_lock_release(&slocks[i]);
			break;
		}
		spin_lock_release(&slocks[i]);
	}
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
		semaphore_init(&sema[i]);
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
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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

void semaphore_init(semaphore_t *semaphore)
{
	mutex_init(&semaphore->mutex);
	semaphore_destroy(semaphore);
}

void semaphore_destroy(semaphore_t *semaphore)
{
	semaphore->key = -1;
	semaphore->value = 0;
	list_destroy(&semaphore->wait_list);
	semaphore->pid = 0;
	mutex_destroy(&semaphore->mutex);
}

void do_semaphore_destroy(int sema_idx)
{
	mutex_lock_acquire(&sema[sema_idx].mutex);
	semaphore_destroy(&sema[sema_idx]);
}

void init_conditions(void)
{
	for (int i = 0; i < CONDITION_NUM; ++i) {
		condition_init(&cond[i]);
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
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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

void condition_init(condition_t *condition)
{
	mutex_init(&condition->mutex);
	condition_destroy(condition);
}

void condition_destroy(condition_t *condition)
{
	condition->key = -1;
	list_destroy(&condition->wait_list);
	condition->pid = 0;
	mutex_destroy(&condition->mutex);
}

void do_condition_destroy(int cond_idx)
{
	mutex_lock_acquire(&cond[cond_idx].mutex);
	condition_destroy(&cond[cond_idx]);
}

void init_barriers(void)
{
	for (int i = 0; i < BARRIER_NUM; ++i) {
		barrier_init(&bar[i]);
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
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
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

void barrier_init(barrier_t *barrier)
{
	mutex_init(&barrier->mutex);
	barrier_destroy(barrier);
}

void barrier_destroy(barrier_t *barrier)
{
	barrier->cnt = 0;
	barrier->goal = 0;
	barrier->key = -1;
	list_destroy(&barrier->wait_list);
	mutex_destroy(&barrier->mutex);
}

void do_barrier_destroy(int bar_idx)
{
	mutex_lock_acquire(&bar[bar_idx].mutex);
	barrier_destroy(&bar[bar_idx]);
}

void mbox_init(mailbox_t *mailbox)
{
	mutex_init(&mailbox->mutex);
	mbox_destroy(mailbox);
}

void mbox_destroy(mailbox_t *mailbox)
{
	mailbox->size = 0;
	mailbox->front = 0;
	mailbox->tail = 0;
	mailbox->user_num = 0;
	list_destroy(&mailbox->reader_wait_list);
	list_destroy(&mailbox->writer_wait_list);
	mutex_destroy(&mailbox->mutex);
}

void init_mbox(void)
{
	for (int i = 0; i < MBOX_NUM; ++i) {
		mbox_init(&mbox[i]);
	}
}

int do_mbox_open(char *name)
{
	for (int i = 0; i < MBOX_NUM; ++i) {
		mutex_lock_acquire(&mbox[i].mutex);
		if (strcmp(name, mbox[i].name) == 0) {
			++mbox[i].user_num;
			mutex_lock_release(&mbox[i].mutex);
			return i;
		}
		mutex_lock_release(&mbox[i].mutex);
	}
	int ret = -1;
	for (int i = 0; i < MBOX_NUM; ++i) {
		mutex_lock_acquire(&mbox[i].mutex);
		if (mbox[i].user_num == 0 || strcmp(name, mbox[i].name) == 0) {
			++mbox[i].user_num;
			strcpy(mbox[i].name, name);
			mutex_lock_release(&mbox[i].mutex);
			ret = i;
			break;
		}
		mutex_lock_release(&mbox[i].mutex);
	}
	return ret;
}

void do_mbox_close(int mbox_idx)
{
	mutex_lock_acquire(&mbox[mbox_idx].mutex);
	if (--mbox[mbox_idx].user_num == 0) {
		mbox_destroy(&mbox[mbox_idx]);
	} else {
		mutex_lock_release(&mbox[mbox_idx].mutex);
	}
}

int do_mbox_send(int mbox_idx, void *msg, int msg_length)
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	int blocked = 0;
	mutex_lock_acquire(&mbox[mbox_idx].mutex);
	while (msg_length + mbox[mbox_idx].size > MAX_MBOX_LENGTH) {
		blocked = 1;
		do_block(&current_running->list,
			 &mbox[mbox_idx].writer_wait_list);
		mutex_lock_release(&mbox[mbox_idx].mutex);
		do_scheduler();
		mutex_lock_acquire(&mbox[mbox_idx].mutex);
	}
	mbox[mbox_idx].size += msg_length;
	char *src = msg;
	for (int i = 0; i < msg_length; ++i) {
		mbox[mbox_idx].buf[mbox[mbox_idx].tail++] = src[i];
		if (mbox[mbox_idx].tail == MAX_MBOX_LENGTH)
			mbox[mbox_idx].tail = 0;
	}
	while (!list_empty(&mbox[mbox_idx].reader_wait_list)) {
		do_unblock(list_front(&mbox[mbox_idx].reader_wait_list));
	}
	mutex_lock_release(&mbox[mbox_idx].mutex);
	return blocked;
}

int do_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	int blocked = 0;
	mutex_lock_acquire(&mbox[mbox_idx].mutex);
	while (mbox[mbox_idx].size < msg_length) {
		blocked = 1;
		do_block(&current_running->list,
			 &mbox[mbox_idx].reader_wait_list);
		mutex_lock_release(&mbox[mbox_idx].mutex);
		do_scheduler();
		mutex_lock_acquire(&mbox[mbox_idx].mutex);
	}
	mbox[mbox_idx].size -= msg_length;
	char *dest = msg;
	for (int i = 0; i < msg_length; ++i) {
		dest[i] = mbox[mbox_idx].buf[mbox[mbox_idx].front++];
		if (mbox[mbox_idx].front == MAX_MBOX_LENGTH)
			mbox[mbox_idx].front = 0;
	}
	while (!list_empty(&mbox[mbox_idx].writer_wait_list)) {
		do_unblock(list_front(&mbox[mbox_idx].writer_wait_list));
	}
	mutex_lock_release(&mbox[mbox_idx].mutex);
	return blocked;
}
