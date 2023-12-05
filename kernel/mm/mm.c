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
	for (int i = 0; i < SHARE_PAGE_NUMS; ++i) {
		spin_lock_init(&sharepgcb[i].lock);
		sharepgcb[i].key = -1;
		sharepgcb[i].user_num = 0;
		sharepgcb[i].addr = 0;
	}
	for (int i = 0; i < MEM_PAGE_NUMS; ++i) {
		spin_lock_init(&mempgcb[i].lock);
		mempgcb[i].pid = 0;
		mempgcb[i].status = FREE;
		mempgcb[i].vaddr = 0;
	}
	for (int i = 0; i < PAGE_NUMS; ++i) {
		ptr_t addr = i * PAGE_SIZE + FREEMEM_KERNEL;
		spin_lock_init(&pgcb[i].lock);
		pgcb[i].addr = addr;
		pgcb[i].status = FREE;
		pgcb[i].pin = UNPINNED;
		pgcb[i].pid = 0;
		pgcb[i].vaddr = 0;
	}
}

ptr_t allocPage(int pid, uint64_t vaddr, pg_pin_status_t pin)
{
	pgcb_t *pg = NULL;
	for (int i = 0; i < PAGE_NUMS; ++i) {
		if (spin_lock_try_acquire(&pgcb[i].lock)) {
			if (pgcb[i].status == FREE) {
				pg = &pgcb[i];
				break;
			}
			spin_lock_release(&pgcb[i].lock);
		}
	}
	if (pg == NULL) {
		pg = swapOut();
	}
	pg->status = ALLOC;
	pg->pid = pid;
	pg->vaddr = vaddr;
	pg->pin = pin;
	pg->cnt = 1;
	clear_pgdir(pg->addr);
	spin_lock_release(&pg->lock);
	printl("process %d alloc page %d, vaddr = %lx, pin = %d\n", pid,
	       addr2idx(pg->addr), vaddr, (pin == PINNED));
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
	int idx = -1;
	for (int i = 0; i < SHARE_PAGE_NUMS; ++i) {
		spin_lock_acquire(&sharepgcb[i].lock);
		if (key == sharepgcb[i].key) {
			idx = i;
			break;
		}
		spin_lock_release(&sharepgcb[i].lock);
	}
	if (idx == -1) {
		for (int i = 0; i < SHARE_PAGE_NUMS; ++i) {
			spin_lock_acquire(&sharepgcb[i].lock);
			if (key == sharepgcb[i].key || sharepgcb[i].key == -1) {
				sharepgcb[i].key = key;
				idx = i;
				break;
			}
			spin_lock_release(&sharepgcb[i].lock);
		}
	}
	pcb_t *current_running = get_current_running();
	++sharepgcb[idx].user_num;
	if (sharepgcb[idx].user_num == 1) {
		sharepgcb[idx].addr = allocPage(0, 0, PINNED);
	}
	uint64_t va = -1;
	for (uint64_t i = PAGE_SIZE; i < 0x0000003ffffffffflu; i += PAGE_SIZE) {
		if (!valid_va(i, current_running->pagedir)) {
			va = i;
			break;
		}
	}
	map_page(va, kva2pa(sharepgcb[idx].addr), current_running->pagedir,
		 current_running->pid);
	spin_lock_release(&sharepgcb[idx].lock);
	return va;
}

void shm_page_dt(uintptr_t addr)
{
	// TODO [P4-task4] shm_page_dt:
	pcb_t *current_running = get_current_running();
	ptr_t kvaddr =
		pa2kva(get_pa(*getEntry(current_running->pagedir, addr)));
	*getEntry(current_running->pagedir, addr) = 0;
	int idx = -1;
	for (int i = 0; i < SHARE_PAGE_NUMS; ++i) {
		spin_lock_acquire(&sharepgcb[i].lock);
		if (kvaddr == sharepgcb[i].addr) {
			idx = i;
			break;
		}
		spin_lock_release(&sharepgcb[i].lock);
	}
	--sharepgcb[idx].user_num;
	if (sharepgcb[idx].user_num == 0) {
		freePage(&pgcb[addr2idx(sharepgcb[idx].addr)]);
		sharepgcb[idx].addr = 0;
		sharepgcb[idx].key = -1;
	}
	spin_lock_release(&sharepgcb[idx].lock);
}