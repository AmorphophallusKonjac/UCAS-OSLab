#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)
    j exec
exit:
    li a7, 1
    li a0, 0
    li a1, 0
    li a2, 0
    li a3, 0
    li a4, 0
    ecall
exec:
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
    /* TODO: [P3] call sys_exit */
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
