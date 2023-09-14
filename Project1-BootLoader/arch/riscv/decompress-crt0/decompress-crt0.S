#include <asm.h>
#include <csr.h>

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)

    /* Mask all interrupts */
    csrw CSR_SIE, zero
    csrw CSR_SIP, zero
    
    /* TODO: [p1-task5] setup C runtime environment for the user program */
    
    // clear bss
    la s0, __bss_start
    la s1, __BSS_END__
    bge s0, s1, bss_clear_done

bss_clear:
    sw zero, 0(s0)
    addi s0, s0, 4
    blt s0, s1, bss_clear
bss_clear_done:

    /* TODO: [p1-task5] setup C environment */
    li sp, KERNEL_STACK

    /* TODO: [p1-task5] enter main function */
    call main

    /* TODO: [p1-task5] return to bootblock */
    ret


    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
