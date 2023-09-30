#include <asm/biosdef.h>

#define KERNEL_STACK		0x50500000

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

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
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

