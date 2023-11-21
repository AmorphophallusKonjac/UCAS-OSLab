#include <os/page.h>

void map_big_page(uint64_t va, uint64_t pa, PTE *pgdir)
{
	va &= VA_MASK;
	uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	uint64_t vpn1 = (vpn2 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
	if (pgdir[vpn2] == 0) {
		// alloc a new second-level page directory
		set_pfn(&pgdir[vpn2], kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
		set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(pgdir[vpn2])));
	}
	PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
	set_pfn(&pmd[vpn1], pa >> NORMAL_PAGE_SHIFT);
	set_attribute(&pmd[vpn1], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
					  _PAGE_EXEC | _PAGE_ACCESSED |
					  _PAGE_DIRTY);
}

void map_page(uint64_t va, uint64_t pa, PTE *firstPgdir)
{
	va &= VA_MASK;
	uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	uint64_t vpn1 = (vpn2 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
	uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT));
	if (firstPgdir[vpn2] == 0) {
		// alloc a new second-level page directory
		set_pfn(&firstPgdir[vpn2],
			kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
		set_attribute(&firstPgdir[vpn2], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(firstPgdir[vpn2])));
	}
	PTE *secondPgdir = (PTE *)pa2kva(get_pa(firstPgdir[vpn2]));
	if (secondPgdir[vpn1] == 0) {
		set_pfn(&secondPgdir[vpn1],
			kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
		set_attribute(&secondPgdir[vpn1], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(secondPgdir[vpn1])));
	}
	PTE *thirdPgdit = (PTE *)pa2kva(get_pa(secondPgdir[vpn1]));
	set_pfn(&thirdPgdit[vpn0], pa >> NORMAL_PAGE_SHIFT);
	set_attribute(&thirdPgdit[vpn0], _PAGE_PRESENT | _PAGE_READ |
						 _PAGE_WRITE | _PAGE_EXEC |
						 _PAGE_ACCESSED | _PAGE_DIRTY);
}

PTE *initPgtable()
{
	PTE *pgdir = (PTE *)allocPage();
	clear_pgdir((uintptr_t)pgdir);
	for (uint64_t kva = 0xffffffc050000000lu; kva < 0xffffffc060000000lu;
	     kva += 0x200000lu) {
		map_big_page(kva, kva2pa(kva), pgdir);
	}
	return pgdir;
}