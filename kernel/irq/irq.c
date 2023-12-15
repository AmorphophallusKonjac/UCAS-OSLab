#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/smp.h>
#include <os/page.h>
#include <os/net.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>
#include <e1000.h>
#include <plic.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];
spin_lock_t copy_on_write_lock;

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
	// TODO: [p2-task3] & [p2-task4] interrupt handler.
	// call corresponding handler by the value of `scause`
	if (scause & (1UL << 63)) {
		irq_table[scause ^ (1UL << 63)](regs, stval, scause);
	} else {
		exc_table[scause](regs, stval, scause);
	}
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
	// TODO: [p2-task4] clock interrupt handler.
	// Note: use bios_set_timer to reset the timer and remember to reschedule
	yield();
}

void load_page_fault_handler(regs_context_t *regs, uint64_t stval,
			     uint64_t scause)
{
	pcb_t *current_running = get_current_running();
	int pid = current_running->pid;
	PTE *firstPgdir = current_running->pagedir;
	uint64_t va = stval;
	va &= VA_MASK;
	uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	uint64_t vpn1 = (vpn2 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
	uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT));
	if (get_attribute(firstPgdir[vpn2], _PAGE_PRESENT) == 0) {
		set_pfn(&firstPgdir[vpn2],
			kva2pa(allocPage(pid, (va >> 12) << 12, PINNED)) >>
				NORMAL_PAGE_SHIFT);
		set_attribute(&firstPgdir[vpn2], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(firstPgdir[vpn2])));
	}
	PTE *secondPgdir = (PTE *)pa2kva(get_pa(firstPgdir[vpn2]));
	if (get_attribute(secondPgdir[vpn1], _PAGE_PRESENT) == 0) {
		set_pfn(&secondPgdir[vpn1],
			kva2pa(allocPage(pid, (va >> 12) << 12, PINNED)) >>
				NORMAL_PAGE_SHIFT);
		set_attribute(&secondPgdir[vpn1], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(secondPgdir[vpn1])));
	}
	PTE *thirdPgdir = (PTE *)pa2kva(get_pa(secondPgdir[vpn1]));
	if (get_attribute(thirdPgdir[vpn0], _PAGE_PRESENT) == 0) {
		if (get_attribute(thirdPgdir[vpn0], _PAGE_SOFT_OUT) != 0) {
			set_pfn(&thirdPgdir[vpn0],
				kva2pa(swapIn((va >> NORMAL_PAGE_SHIFT)
					      << NORMAL_PAGE_SHIFT)) >>
					NORMAL_PAGE_SHIFT);
			set_attribute(&thirdPgdir[vpn0],
				      get_attribute(thirdPgdir[vpn0], 1023) ^
					      _PAGE_SOFT_OUT ^ _PAGE_PRESENT);
		} else {
			set_pfn(&thirdPgdir[vpn0],
				kva2pa(allocPage(pid, (va >> 12) << 12,
						 UNPINNED)) >>
					NORMAL_PAGE_SHIFT);
			set_attribute(&thirdPgdir[vpn0],
				      _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
					      _PAGE_EXEC | _PAGE_USER);
		}
	} else {
		set_attribute(&thirdPgdir[vpn0],
			      _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
				      _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_USER);
	}
}

void store_page_fault_handler(regs_context_t *regs, uint64_t stval,
			      uint64_t scause)
{
	pcb_t *current_running = get_current_running();
	int pid = current_running->pid;
	PTE *firstPgdir = current_running->pagedir;
	uint64_t va = stval;
	va &= VA_MASK;
	uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	uint64_t vpn1 = (vpn2 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
	uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT));
	if (get_attribute(firstPgdir[vpn2], _PAGE_PRESENT) == 0) {
		set_pfn(&firstPgdir[vpn2],
			kva2pa(allocPage(pid, (va >> 12) << 12, PINNED)) >>
				NORMAL_PAGE_SHIFT);
		set_attribute(&firstPgdir[vpn2], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(firstPgdir[vpn2])));
	}
	PTE *secondPgdir = (PTE *)pa2kva(get_pa(firstPgdir[vpn2]));
	if (get_attribute(secondPgdir[vpn1], _PAGE_PRESENT) == 0) {
		set_pfn(&secondPgdir[vpn1],
			kva2pa(allocPage(pid, (va >> 12) << 12, PINNED)) >>
				NORMAL_PAGE_SHIFT);
		set_attribute(&secondPgdir[vpn1], _PAGE_PRESENT);
		clear_pgdir(pa2kva(get_pa(secondPgdir[vpn1])));
	}
	PTE *thirdPgdir = (PTE *)pa2kva(get_pa(secondPgdir[vpn1]));
	if (get_attribute(thirdPgdir[vpn0], _PAGE_PRESENT) == 0) {
		if (get_attribute(thirdPgdir[vpn0], _PAGE_SOFT_OUT) != 0) {
			set_pfn(&thirdPgdir[vpn0],
				kva2pa(swapIn((va >> NORMAL_PAGE_SHIFT)
					      << NORMAL_PAGE_SHIFT)) >>
					NORMAL_PAGE_SHIFT);
			set_attribute(&thirdPgdir[vpn0],
				      get_attribute(thirdPgdir[vpn0], 1023) ^
					      _PAGE_SOFT_OUT ^ _PAGE_PRESENT);
		} else {
			set_pfn(&thirdPgdir[vpn0],
				kva2pa(allocPage(pid, (va >> 12) << 12,
						 UNPINNED)) >>
					NORMAL_PAGE_SHIFT);
			set_attribute(&thirdPgdir[vpn0],
				      _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
					      _PAGE_EXEC | _PAGE_USER);
		}
	} else {
		if (get_attribute(thirdPgdir[vpn0], _PAGE_SOFT_FORK) != 0) {
			spin_lock_acquire(&copy_on_write_lock);
			printl("pid %d get copy on write lock\n",
			       current_running->pid);
			if (get_attribute(thirdPgdir[vpn0], _PAGE_SOFT_FORK) ==
			    0) {
				spin_lock_release(&copy_on_write_lock);
				printl("pid %d release copy on write lock\n",
				       current_running->pid);
				return;
			}
			PTE *entry = NULL;
			for (int i = 0; i < NUM_MAX_TASK; ++i) {
				if (&pcb[i] == current_running)
					continue;
				spin_lock_acquire(&pcb[i].lock);
				if (pcb[i].status != TASK_EXITED &&
				    valid_va(va, pcb[i].pagedir)) {
					entry = getEntry(pcb[i].pagedir, va);
					if (get_attribute(*entry,
							  _PAGE_SOFT_FORK) !=
						    0 &&
					    get_pa(*entry) ==
						    get_pa(thirdPgdir[vpn0])) {
						spin_lock_release(&pcb[i].lock);
						break;
					}
				}
				spin_lock_release(&pcb[i].lock);
			}
			ptr_t old_addr = pa2kva(get_pa(thirdPgdir[vpn0]));
			printl("pid %d copy page %d va %lx\n",
			       current_running->pid, addr2idx(old_addr), va);
			spin_lock_acquire(&pgcb[addr2idx(old_addr)].lock);
			--pgcb[addr2idx(old_addr)].cnt;
			int pin = pgcb[addr2idx(old_addr)].pin;
			ptr_t addr = allocPage(current_running->pid,
					       (va >> NORMAL_PAGE_SHIFT)
						       << NORMAL_PAGE_SHIFT,
					       pin);
			memcpy((uint8_t *)addr, (uint8_t *)old_addr, PAGE_SIZE);
			set_pfn(&thirdPgdir[vpn0],
				kva2pa(addr) >> NORMAL_PAGE_SHIFT);
			set_attribute(&thirdPgdir[vpn0],
				      get_attribute(thirdPgdir[vpn0],
						    1023 ^ _PAGE_SOFT_FORK) |
					      _PAGE_WRITE);
			set_attribute(entry,
				      get_attribute(*entry,
						    1023 ^ _PAGE_SOFT_FORK) |
					      _PAGE_WRITE);
			spin_lock_release(&pgcb[addr2idx(old_addr)].lock);
			spin_lock_release(&copy_on_write_lock);
			printl("pid %d release copy on write lock\n",
			       current_running->pid);
		} else {
			set_attribute(&thirdPgdir[vpn0],
				      _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
					      _PAGE_EXEC | _PAGE_ACCESSED |
					      _PAGE_DIRTY | _PAGE_USER);
		}
	}
}

void inst_page_fault_handler(regs_context_t *regs, uint64_t stval,
			     uint64_t scause)
{
	pcb_t *current_running = get_current_running();
	PTE *firstPgdir = current_running->pagedir;
	uint64_t va = stval;
	va &= VA_MASK;
	printl("pid %d inst_page_fault\n", current_running->pid);
	printl("va = %lx\n", va);
	uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	uint64_t vpn1 = (vpn2 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
	uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
			(va >> (NORMAL_PAGE_SHIFT));
	if (get_attribute(firstPgdir[vpn2], _PAGE_PRESENT) == 0) {
		printl("vpn2 error\n");
		assert(0);
	}
	PTE *secondPgdir = (PTE *)pa2kva(get_pa(firstPgdir[vpn2]));
	if (get_attribute(secondPgdir[vpn1], _PAGE_PRESENT) == 0) {
		printl("vpn1 error\n");
		assert(0);
	}
	PTE *thirdPgdir = (PTE *)pa2kva(get_pa(secondPgdir[vpn1]));
	if (get_attribute(thirdPgdir[vpn0], _PAGE_PRESENT) == 0) {
		printl("vpn0 error\n");
		assert(0);
	} else {
		printl("unknown error\n");
		assert(0);
	}
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
	// TODO: [p5-task3] external interrupt handler.
	// Note: plic_claim and plic_complete will be helpful ...
	local_flush_dcache();
	uint32_t id = plic_claim();
	if (id == PLIC_E1000_QEMU_IRQ || id == PLIC_E1000_PYNQ_IRQ) {
		net_handle_irq();
	}
	plic_complete(id);
}

void init_exception()
{
	/* TODO: [p2-task3] initialize exc_table */
	/* NOTE: handle_syscall, handle_other, etc.*/
	for (int i = 0; i < EXCC_COUNT; ++i) {
		exc_table[i] = (handler_t)handle_other;
	}
	exc_table[EXCC_SYSCALL] = (handler_t)handle_syscall;
	exc_table[EXCC_LOAD_PAGE_FAULT] = (handler_t)load_page_fault_handler;
	exc_table[EXCC_STORE_PAGE_FAULT] = (handler_t)store_page_fault_handler;
	exc_table[EXCC_INST_PAGE_FAULT] = (handler_t)inst_page_fault_handler;
	/* TODO: [p2-task4] initialize irq_table */
	/* NOTE: handle_int, handle_other, etc.*/
	for (int i = 0; i < IRQC_COUNT; ++i) {
		irq_table[i] = (handler_t)handle_other;
	}
	irq_table[IRQC_S_TIMER] = (handler_t)handle_irq_timer;
	irq_table[IRQC_S_EXT] = (handler_t)handle_irq_ext;
	/* TODO: [p2-task3] set up the entrypoint of exceptions */
	setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
	char *reg_name[] = { "zero ", " ra  ", " sp  ", " gp  ", " tp  ",
			     " t0  ", " t1  ", " t2  ", "s0/fp", " s1  ",
			     " a0  ", " a1  ", " a2  ", " a3  ", " a4  ",
			     " a5  ", " a6  ", " a7  ", " s2  ", " s3  ",
			     " s4  ", " s5  ", " s6  ", " s7  ", " s8  ",
			     " s9  ", " s10 ", " s11 ", " t3  ", " t4  ",
			     " t5  ", " t6  " };
	for (int i = 0; i < 32; i += 3) {
		for (int j = 0; j < 3 && i + j < 32; ++j) {
			printk("%s : %016lx ", reg_name[i + j],
			       regs->regs[i + j]);
		}
		printk("\n\r");
	}
	printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r", regs->sstatus,
	       regs->sbadaddr, regs->scause);
	printk("sepc: 0x%lx\n\r", regs->sepc);
	printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
	printk("current running cpu: %d\n", get_current_cpu_id());
	assert(0);
}
