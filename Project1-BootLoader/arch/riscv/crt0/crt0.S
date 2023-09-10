#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */

    // clear bss
    la s0, __bss_start
    la s1, __BSS_END__
    bge s0, s1, bss_clear_done

bss_clear:
    sw zero, 0(s0)
    addi s0, s0, 4
    blt s0, s1, bss_clear
bss_clear_done:

    // store ra
    addi sp, sp, -8
    sd ra, 0(sp)

    /* TODO: [p1-task3] enter main function */
    call main

    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    ld ra, 0(sp)
    addi sp, sp, 8
    ret


    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
