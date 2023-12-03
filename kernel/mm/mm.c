#include <os/mm.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/page.h>
#include <assert.h>

// NOTE: A/C-core

int addr2idx(ptr_t addr)
{
	return (addr - FREEMEM_KERNEL) / PAGE_SIZE;
}

int idx2sectorIdx(int idx)
{
	return idx * 8;
}

static ptr_t kernMemCurr = FREEMEM_KERNEL;

void initkmem()
{
	for (int i = 0; i < MEM_PAGE_NUMS; ++i) {
		spin_lock_init(&mempgcb[i].lock);
		mempgcb[i].pid = 0;
		mempgcb[i].status = FREE;
		mempgcb[i].vaddr = 0;
	}
	for (ptr_t i = FREEMEM_KERNEL; i < 0xffffffc060000000lu;
	     i += PAGE_SIZE) {
		int idx = addr2idx(i);
		spin_lock_init(&pgcb[idx].lock);
		pgcb[idx].addr = i;
		pgcb[idx].status = FREE;
		pgcb[idx].pin = UNPINNED;
		pgcb[idx].pid = 0;
		pgcb[idx].vaddr = 0;
	}
}

ptr_t allocPage(int pid, uint64_t vaddr, pg_pin_status_t pin)
{
	pgcb_t *pg = NULL;
	for (int i = 0; i < PAGE_NUMS; ++i) {
		spin_lock_acquire(&pgcb[i].lock);
		if (pgcb[i].status == FREE) {
			pg = &pgcb[i];
			break;
		}
		spin_lock_release(&pgcb[i].lock);
	}
	if (pg == NULL) {
		swapOut();
	}
	pg->status = ALLOC;
	pg->pid = pid;
	pg->vaddr = vaddr;
	pg->pin = pin;
	clear_pgdir(pg->addr);
	spin_lock_release(&pg->lock);
	return pg->addr;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
	// align LARGE_PAGE_SIZE
	ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
	largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
	return ret;
}
#endif

void freePage(pgcb_t *pg)
{
	// TODO [P4-task1] (design you 'freePage' here if you need):
	spin_lock_acquire(&pg->lock);
	clear_pgdir(pg->addr);
	pg->status = FREE;
	pg->pin = UNPINNED;
	pg->pid = 0;
	pg->vaddr = 0;
	spin_lock_release(&pg->lock);
}

void *kmalloc(size_t size)
{
	// TODO [P4-task1] (design you 'kmalloc' here if you need):
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
	// TODO [P4-task1] share_pgtable:
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
	// TODO [P4-task1] alloc_page_helper:
}

uintptr_t shm_page_get(int key)
{
	// TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
	// TODO [P4-task4] shm_page_dt:
}