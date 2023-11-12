/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS Author :
 * Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Thread Lock
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

#ifndef INCLUDE_LOCK_H_
#define INCLUDE_LOCK_H_

#include <os/list.h>

#define LOCK_NUM 16

typedef enum {
  UNLOCKED,
  LOCKED,
} lock_status_t;

typedef struct spin_lock {
  volatile lock_status_t status;
  int pid;
} spin_lock_t;

typedef struct mutex_lock {
  spin_lock_t lock;
  list_head block_queue;
  int key;
  int pid;
} mutex_lock_t;

void init_locks(void);

void spin_lock_init(spin_lock_t *lock);
int spin_lock_try_acquire(spin_lock_t *lock);
void spin_lock_acquire(spin_lock_t *lock);
void spin_lock_release(spin_lock_t *lock);

void mutex_init(mutex_lock_t *mutex);
void mutex_destroy(mutex_lock_t *mutex);
void mutex_lock_acquire(mutex_lock_t *mutex);
void mutex_lock_release(mutex_lock_t *mutex);
int do_mutex_lock_init(int key);
void do_mutex_lock_acquire(int mlock_idx);
void do_mutex_lock_release(int mlock_idx);
void do_destroy_pthread_lock(int pid);

/************************************************************/
typedef struct barrier {
  // TODO [P3-TASK2 barrier]
  int key, cnt, goal, pid;
  spin_lock_t lock;
  list_head wait_list;
} barrier_t;

#define BARRIER_NUM 16

void barrier_init(barrier_t *barrier);
void barrier_destroy(barrier_t *barrier);
void init_barriers(void);
int do_barrier_init(int key, int goal);
void do_barrier_wait(int bar_idx);
void do_barrier_destroy(int bar_idx);

typedef struct condition {
  // TODO [P3-TASK2 condition]
  int key, pid;
  mutex_lock_t mutex;
  list_head wait_list;
} condition_t;

#define CONDITION_NUM 16

void condition_init(condition_t *condition);
void condition_destroy(condition_t *condition);
void init_conditions(void);
int do_condition_init(int key);
void do_condition_wait(int cond_idx, int mutex_idx);
void do_condition_signal(int cond_idx);
void do_condition_broadcast(int cond_idx);
void do_condition_destroy(int cond_idx);

typedef struct semaphore {
  // TODO [P3-TASK2 semaphore]
  int key, value, pid;
  spin_lock_t lock;
  list_head wait_list;
} semaphore_t;

#define SEMAPHORE_NUM 16

void semaphore_init(semaphore_t *semaphore);
void semaphore_destroy(semaphore_t *semaphore);
void init_semaphores(void);
int do_semaphore_init(int key, int init);
void do_semaphore_up(int sema_idx);
void do_semaphore_down(int sema_idx);
void do_semaphore_destroy(int sema_idx);

#define MAX_MBOX_LENGTH (64)

typedef struct mailbox {
  // TODO [P3-TASK2 mailbox]
  list_head reader_wait_list, writer_wait_list;
  char name[32];
  char buf[MAX_MBOX_LENGTH];
  int size, front, tail, user_num;
  spin_lock_t lock;
} mailbox_t;

#define MBOX_NUM 16

void mbox_init(mailbox_t *mailbox);
void mbox_destroy(mailbox_t *mailbox);
void init_mbox(void);
int do_mbox_open(char *name);
void do_mbox_close(int mbox_idx);
int do_mbox_send(int mbox_idx, void *msg, int msg_length);
int do_mbox_recv(int mbox_idx, void *msg, int msg_length);

/************************************************************/

#endif
