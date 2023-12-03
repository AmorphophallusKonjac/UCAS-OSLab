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

#define SECTOR_SIZE 512
#define SECTOR_BASE 10000

PTE *initPgtable(int pid);
void map_page(uint64_t va, uint64_t pa, PTE *firstPgdir, int pid);
void unmapBoot(void);
void unmapPageDir(int pid);

pgcb_t *swapOut(void);

#endif