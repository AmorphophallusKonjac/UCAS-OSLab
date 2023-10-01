#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

extern void ret_from_exception();
#define VERSION_BUF 50
#define OS_SIZE_LOC 0x502001fc
#define STACK_PAGE_NUM 1

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

// [p1-Task3] Task num
uint16_t task_num;

static int bss_check(void) {
    for (int i = 0; i < VERSION_BUF; ++i) {
        if (buf[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void) {
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR] = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ] = (long (*)())sd_read;
    jmptab[SD_WRITE] = (long (*)())sd_write;
    jmptab[QEMU_LOGGING] = (long (*)())qemu_logging;
    jmptab[SET_TIMER] = (long (*)())set_timer;
    jmptab[READ_FDT] = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR] = (long (*)())screen_move_cursor;
    jmptab[PRINT] = (long (*)())printk;
    jmptab[YIELD] = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT] = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ] = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE] = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.
    jmptab[WRITE] = (long (*)())screen_write;
    jmptab[REFLUSH] = (long (*)())screen_reflush;
}

static void init_task_info(void) {
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    uint64_t taskinfo_size = *(uint32_t *)(OS_SIZE_LOC - 0x8);
    uint64_t taskinfo_offset = *(uint32_t *)(OS_SIZE_LOC - 0xC);
    taskinfo_offset = taskinfo_offset % (uint64_t)(0x200);
    task_num = *(uint16_t *)(OS_SIZE_LOC + 0x2);
    memcpy((uint8_t *)tasks, (uint8_t *)(0x52000000 + taskinfo_offset), taskinfo_size);
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb) {
    /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t)); 
    for (int i = 0; i < 14; ++i) 
        pt_switchto->regs[i] = (reg_t) 0;
    pt_switchto->regs[0] = (reg_t) entry_point;         //ra
    pt_switchto->regs[1] = (reg_t) pt_switchto;         //sp

    pcb->kernel_sp = (reg_t) pt_switchto;
    pcb->user_sp = user_stack;
}

static void init_pcb(void) {
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    char needed_task_name[][32] = {"print1", "fly", "print2", "lock1", "lock2"};

    for(int i=1; i<=sizeof(needed_task_name)/32; i++){
        pcb[i].pid = process_id++;
        pcb[i].status = TASK_READY;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = 0;
        pcb[i].wakeup_time = 0;
        init_pcb_stack(
            allocKernelPage(STACK_PAGE_NUM) + STACK_PAGE_NUM * PAGE_SIZE,
            allocUserPage(STACK_PAGE_NUM) + STACK_PAGE_NUM * PAGE_SIZE,
            from_name_load_task_img(needed_task_name[i-1]), 
            pcb+i
        );
        list_push(&ready_queue, &pcb[i].list);
    }
    pcb[0]=pid0_pcb;

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running=pcb+0;
    current_running->status=TASK_BLOCKED; // to stop pcb0 from being pushed into ready_queue
}

static void init_syscall(void) {
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

int main(void) {
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i) {
        buf[i] = output_str[i];
        if (buf[i] == '_') {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);
    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1) {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}
