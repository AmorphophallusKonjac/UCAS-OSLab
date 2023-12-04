/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Memory Management
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
#ifndef MM_H
#define MM_H

#define TEST_SWAP

#ifdef TEST_SWAP
#define PAGE_NUMS 50
#else
#define PAGE_NUMS 57344
#endif

#define SHARE_PAGE_NUMS 16

#define MEM_PAGE_NUMS 28672

#include <os/list.h>
#include <os/lock.h>
#include <pgtable.h>
#include <type.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL INIT_KERNEL_STACK

/* Rounding; only works for n = power of two */
#define ROUND(a, n) (((((uint64_t)(a)) + (n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

typedef enum { PINNED, UNPINNED } pg_pin_status_t;
typedef enum { FREE, ALLOC } pg_status_t;

typedef struct pgcb {
	spin_lock_t lock;
	ptr_t addr;
	pg_status_t status;
	pg_pin_status_t pin;

	int pid;
	uint64_t vaddr;
} pgcb_t;

pgcb_t pgcb[PAGE_NUMS];

typedef struct mempgcb {
	spin_lock_t lock;
	pg_status_t status;
	int pid;
	uint64_t vaddr;
} mempgcb_t;

mempgcb_t mempgcb[MEM_PAGE_NUMS];

typedef struct sharepgcb {
	spin_lock_t lock;
	int key;
	int user_num;
	ptr_t addr;
} sharepgcb_t;

sharepgcb_t sharepgcb[SHARE_PAGE_NUMS];

int addr2idx(ptr_t addr);
int idx2sectorIdx(int idx);

ptr_t allocPage(int pid, uint64_t vaddr, pg_pin_status_t pin);
// TODO [P4-task1] */
void freePage(pgcb_t *pg);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

// TODO [P4-task1] */
extern void *kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir);

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);

void initkmem();

#endif /* MM_H */
