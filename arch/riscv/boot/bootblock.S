#include <asm/biosdef.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50202000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

// [p1-task5] depress function entry (jump here to depress kernel)
.equ decompress, 0x51000000

.equ compressed_kernel, 0x54000000

.text
.global main

main:
	// fence on all memory and I/O
	fence
    // a0 is mhartid
	bnez a0, secondary

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	la a0, msg
	li a7, BIOS_PUTSTR
	jal bios_func_entry

	// TODO: [p1-task2] call BIOS to read decompress in SD card
	li a0, decompress
	li s0, os_size_loc
	lh a1, 0(s0)
	li a2, 1
	li a7, BIOS_SDREAD
	jal bios_func_entry

	// [p1-task5] call BIOS to read compressed_kernel in SD card
	li a0, compressed_kernel
	li s0, os_size_loc
	lh a1, -22(s0)
	lh a2, -24(s0)
	jal bios_func_entry

	// [p1-task5] set stackpoint
	li sp, KERNEL_STACK

	// [p1-task5] call decompress 
	call decompress


	// TODO: [p1-task4] load task-related arguments and pass them to kernel
	li a0, 0x52000000
	li s0, os_size_loc
	lh a1, -2(s0)
	lh a2, -4(s0)
	li a7, BIOS_SDREAD
	jal bios_func_entry

	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	j kernel
	/************************************************************/
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	csrw CSR_SIE, zero
  	csrw CSR_SIP, zero

	la t0, kernel
  	csrw CSR_STVEC, t0

	li t0, SIE_SSIE
	csrs CSR_SIE, t0
	li t0, SR_SIE
  	csrs CSR_SSTATUS, t0 

wait_for_wakeup:
	wfi
	j wait_for_wakeup

	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

