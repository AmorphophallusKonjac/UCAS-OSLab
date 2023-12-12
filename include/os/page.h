#ifndef _PAGE_H_
#define _PAGE_H_

#include <pgtable.h>
#include <type.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/smp.h>
#include <assert.h>
#include <os/ioremap.h>

#define SECTOR_SIZE 512
#define SECTOR_BASE 300

PTE *initPgtable(int pid);
void map_page(uint64_t va, uint64_t pa, PTE *firstPgdir, int pid);
void map_huge_page(uint64_t va, uint64_t pa, PTE *pgdir);
void map_big_page(uint64_t va, uint64_t pa, PTE *pgdir);
void unmapBoot(void);
void unmapPageDir(int pid);

pgcb_t *swapOut(void);
uintptr_t swapIn(uint64_t vaddr);
int valid_va(uint64_t va, PTE *firstPgdir);
PTE *getEntry(PTE *firstPgdir, uint64_t va);
void set_fork(uint64_t va, PTE *firstPgdir);

#endif