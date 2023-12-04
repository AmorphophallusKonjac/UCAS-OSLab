#include <os/page.h>

void map_big_page(uint64_t va, uint64_t pa, PTE *pgdir)
{
	pcb_t *current_running = get_current_running();
	int pid = current_running->pid;
	va &= VA_MASK;
	uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	uint64_t vpn1 = (vpn2 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
	if (pgdir[vpn2] == 0) {
		// alloc a new second-level page directory
		set_pfn(&pgdir[vpn2],
			kva2pa(allocPage(pid, 0, PINNED)) >> NORMAL_PAGE_SHIFT);
		set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(pgdir[vpn2])));
	}
	PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
	set_pfn(&pmd[vpn1], pa >> NORMAL_PAGE_SHIFT);
	set_attribute(&pmd[vpn1], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
					  _PAGE_EXEC | _PAGE_ACCESSED |
					  _PAGE_DIRTY);
}

void map_page(uint64_t va, uint64_t pa, PTE *firstPgdir, int pid)
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
			kva2pa(allocPage(pid, 0, PINNED)) >> NORMAL_PAGE_SHIFT);
		set_attribute(&firstPgdir[vpn2], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(firstPgdir[vpn2])));
	}
	PTE *secondPgdir = (PTE *)pa2kva(get_pa(firstPgdir[vpn2]));
	if (secondPgdir[vpn1] == 0) {
		set_pfn(&secondPgdir[vpn1],
			kva2pa(allocPage(pid, 0, PINNED)) >> NORMAL_PAGE_SHIFT);
		set_attribute(&secondPgdir[vpn1], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(secondPgdir[vpn1])));
	}
	PTE *thirdPgdir = (PTE *)pa2kva(get_pa(secondPgdir[vpn1]));
	set_pfn(&thirdPgdir[vpn0], pa >> NORMAL_PAGE_SHIFT);
	set_attribute(&thirdPgdir[vpn0],
		      _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |
			      _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER);
}

void unmapBoot()
{
	PTE *early_pgdir = (PTE *)pa2kva(PGDIR_PA);
	for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu; pa += 0x200000lu) {
		uint64_t va = pa;
		va &= VA_MASK;
		uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
		early_pgdir[vpn2] = 0;
	}
}

void unmapPageDir(int pid)
{
	for (int i = 0; i < PAGE_NUMS; ++i) {
		spin_lock_acquire(&pgcb[i].lock);
		if (pgcb[i].cnt > 1)
			continue;
		if (pgcb[i].pid == pid) {
			pgcb[i].pid = 0;
			pgcb[i].status = FREE;
			pgcb[i].pin = UNPINNED;
			pgcb[i].vaddr = 0;
		}
		spin_lock_release(&pgcb[i].lock);
	}
	for (int i = 0; i < MEM_PAGE_NUMS; ++i) {
		spin_lock_acquire(&mempgcb[i].lock);
		if (mempgcb[i].pid == pid) {
			mempgcb[i].pid = 0;
			mempgcb[i].status = FREE;
			mempgcb[i].vaddr = 0;
		}
		spin_lock_release(&mempgcb[i].lock);
	}
}

PTE *initPgtable(int pid)
{
	PTE *pgdir = (PTE *)allocPage(pid, 0, PINNED);
	clear_pgdir((uintptr_t)pgdir);
	memcpy((uint8_t *)pgdir, (uint8_t *)pa2kva(PGDIR_PA), 0x1000);
	for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu; pa += 0x200000lu) {
		uint64_t va = pa;
		va &= VA_MASK;
		uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
		pgdir[vpn2] = 0;
	}
	return pgdir;
}

PTE *getEntry(PTE *firstPgdir, uint64_t va)
{
	va &= VA_MASK;
	uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	uint64_t vpn1 = (vpn2 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
	uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT));
	PTE *secondPgdir = (PTE *)pa2kva(get_pa(firstPgdir[vpn2]));
	PTE *thirdPgdir = (PTE *)pa2kva(get_pa(secondPgdir[vpn1]));
	return &thirdPgdir[vpn0];
}

pgcb_t *swapOut()
{
	static int i = 0;
	pgcb_t *pg = NULL;
	while (pg == NULL) {
		++i;
		for (; i < PAGE_NUMS; ++i) {
			spin_lock_acquire(&pgcb[i].lock);
			if ((pgcb[i].status == ALLOC &&
			     pgcb[i].pin == UNPINNED &&
			     pgcb[i].pid !=
				     cpu[get_current_running()->cpuID ^ 1].pid) ||
			    pgcb[i].status == FREE) {
				pg = &pgcb[i];
				break;
			}
			spin_lock_release(&pgcb[i].lock);
		}
		if (i >= PAGE_NUMS)
			i = 0;
	}
	if (pg->status == FREE) {
		return pg;
	} else {
		pcb_t *pcb = pid2pcb(pg->pid);
		PTE *entry = getEntry(pcb->pagedir, pg->vaddr);
		uint64_t bits = get_attribute(*entry, 1023);
		set_attribute(entry, bits ^ _PAGE_PRESENT ^ _PAGE_SOFT_OUT);
		// printl("set %X\n", get_attribute(*entry, 1023));
	}
	mempgcb_t *mempg = NULL;
	int sector_idx = 0;
	while (mempg == NULL) {
		for (int i = 0; i < MEM_PAGE_NUMS; ++i) {
			spin_lock_acquire(&mempgcb[i].lock);
			if (mempgcb[i].status == FREE) {
				mempg = &mempgcb[i];
				sector_idx = i * 8 + SECTOR_BASE;
				printl("page %d swap out to mem page %d\n",
				       addr2idx(pg->addr), i);
				break;
			}
			spin_lock_release(&mempgcb[i].lock);
		}
	}
	bios_sd_write(kva2pa(pg->addr), PAGE_SIZE / SECTOR_SIZE, sector_idx);
	clear_pgdir(pg->addr);
	mempg->status = ALLOC;
	mempg->vaddr = pg->vaddr;
	mempg->pid = pg->pid;
	spin_lock_release(&mempg->lock);
	return pg;
}

uintptr_t swapIn(uint64_t vaddr)
{
	pcb_t *current_running = get_current_running();
	int pid = current_running->pid;
	ptr_t addr = allocPage(pid, vaddr, UNPINNED);
	mempgcb_t *mem_pg = NULL;
	int sector_idx = 0;
	while (mem_pg == NULL) {
		for (int i = 0; i < MEM_PAGE_NUMS; ++i) {
			spin_lock_acquire(&mempgcb[i].lock);
			if (mempgcb[i].status == ALLOC &&
			    mempgcb[i].vaddr == vaddr &&
			    mempgcb[i].pid == pid) {
				mem_pg = &mempgcb[i];
				sector_idx = i * 8 + SECTOR_BASE;
				printl("mem page %d swap in to page %d\n", i,
				       addr2idx(addr));
				break;
			}
			spin_lock_release(&mempgcb[i].lock);
		}
	}
	bios_sd_read(kva2pa(addr), PAGE_SIZE / SECTOR_SIZE, sector_idx);
	mem_pg->vaddr = 0;
	mem_pg->pid = 0;
	mem_pg->status = FREE;
	spin_lock_release(&mem_pg->lock);
	return addr;
}

int valid_va(uint64_t va, PTE *firstPgdir)
{
	va &= VA_MASK;
	uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	uint64_t vpn1 = (vpn2 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
	uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT));
	if (firstPgdir[vpn2] == 0) {
		return 0;
	}
	PTE *secondPgdir = (PTE *)pa2kva(get_pa(firstPgdir[vpn2]));
	if (secondPgdir[vpn1] == 0) {
		return 0;
	}
	PTE *thirdPgdir = (PTE *)pa2kva(get_pa(secondPgdir[vpn1]));
	if (thirdPgdir[vpn0] == 0) {
		return 0;
	}
	return 1;
}