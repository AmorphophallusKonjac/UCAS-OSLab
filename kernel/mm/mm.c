#include <os/mm.h>
#include <os/lock.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

struct freeList {
	struct freeList *next;
};

struct {
	spin_lock_t lock;
	struct freeList *kmem;
} kmem;

void initkmem()
{
	for (ptr_t i = FREEMEM_KERNEL; i < 0xffffffc060000000lu;
	     i += PAGE_SIZE) {
		freePage(i);
	}
}

ptr_t allocPage()
{
	// align PAGE_SIZE
	// ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
	// kernMemCurr = ret + numPage * PAGE_SIZE;
	// return ret;
	struct freeList *ret;
	spin_lock_acquire(&kmem.lock);
	ret = kmem.kmem;
	if (ret)
		kmem.kmem = ret->next;
	spin_lock_release(&kmem.lock);
	for (int i = 0; i < PAGE_SIZE; ++i) {
		((char *)ret)[i] = 0;
	}
	return (ptr_t)ret;
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

void freePage(ptr_t baseAddr)
{
	// TODO [P4-task1] (design you 'freePage' here if you need):
	struct freeList *ret = (struct freeList *)baseAddr;
	spin_lock_acquire(&kmem.lock);
	ret->next = kmem.kmem;
	kmem.kmem = ret;
	spin_lock_release(&kmem.lock);
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