#ifndef _PAGE_H_
#define _PAGE_H_

#include <pgtable.h>
#include <type.h>
#include <os/mm.h>

PTE *initPgtable();
void map_page(uint64_t va, uint64_t pa, PTE *firstPgdir);

#endif