#include <os/ioremap.h>
#include <os/mm.h>
#include <os/page.h>
#include <pgtable.h>
#include <type.h>

#define IOREMAP_HUGE_PAGE
#define IOREMAP_BIG_PAGE

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
// TODO: [p5-task1] map one specific physical region to virtual address
#ifdef IOREMAP_HUGE_PAGE
	void *ret = (void *)(io_base + phys_addr % HUGE_PAGE_SIZE);
	map_huge_page(io_base, phys_addr & ~(HUGE_PAGE_SIZE - 1),
		      (PTE *)pa2kva(PGDIR_PA));
	io_base += HUGE_PAGE_SIZE;
	local_flush_tlb_all();
	return ret;
#elif defined(IOREMAP_BIG_PAGE)
	void *ret = (void *)io_base;
	int page_num =
		(size / BIG_PAGE_SIZE) + ((size % BIG_PAGE_SIZE != 0) ? 1 : 0);
	for (int i = 0; i < page_num; ++i) {
		map_big_page(io_base, phys_addr, (PTE *)pa2kva(PGDIR_PA));
		io_base += BIG_PAGE_SIZE;
		phys_addr += BIG_PAGE_SIZE;
	}
	local_flush_tlb_all();
	return ret;
#else
	uint64_t page_num =
		(size / PAGE_SIZE) + ((size % PAGE_SIZE != 0) ? 1 : 0);
	void *ret = (void *)io_base;
	for (uint64_t i = 0; i < page_num; ++i) {
		map_page(io_base, phys_addr, (PTE *)pa2kva(PGDIR_PA), 0);
		io_base += PAGE_SIZE;
		phys_addr += PAGE_SIZE;
	}
	local_flush_tlb_all();
	return ret;
#endif
}

// void *ioremap(unsigned long phys_addr, unsigned long size)
// {
// 	uint64_t va = io_base & VA_MASK;
// 	PTE *pgdir_t = (PTE *)pa2kva(PGDIR_PA);
// 	uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS +
// 				PPN_BITS)); //页目录虚地址

// 	set_pfn(&pgdir_t[vpn2],
// 		(phys_addr & ~(HUGE_PAGE_SIZE - 1)) >>
// 			NORMAL_PAGE_SHIFT); //allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
// 	//set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
// 	set_attribute(&pgdir_t[vpn2], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
// 					      _PAGE_EXEC | _PAGE_ACCESSED |
// 					      _PAGE_DIRTY);
// 	// TODO: [p5-task1] map one specific physical region to virtual address

// 	io_base += HUGE_PAGE_SIZE;

// 	local_flush_tlb_all();

// 	return (void *)(io_base - HUGE_PAGE_SIZE +
// 			(phys_addr & (HUGE_PAGE_SIZE - 1)));
// }

void iounmap(void *io_addr)
{
	// TODO: [p5-task1] a very naive iounmap() is OK
	// maybe no one would call this function?
}
