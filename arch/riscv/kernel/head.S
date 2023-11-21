/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define CORE1_KERNEL_STACK		0xffffffc050500000
#define CORE2_KERNEL_STACK    0xffffffc050501000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr a0, CSR_MHARTID

  bnez a0, secondary

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la s0, __bss_start
  la s1, __BSS_END__
  bge s0, s1, bss_clear_done

bss_clear:
  sw zero, 0(s0)
  addi s0, s0, 4
  blt s0, s1, bss_clear
bss_clear_done:

  /* TODO: [p1-task2] setup C environment */
  li sp, CORE1_KERNEL_STACK
  j main

secondary:
  li sp, CORE2_KERNEL_STACK
  j main

loop:
  wfi
  j loop

END(_start)
