#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50
#define OS_SIZE_LOC 0x502001fc

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

// [p1-Task3] Task num
uint16_t task_num;

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    uint64_t taskinfo_size = *(uint32_t *)(OS_SIZE_LOC - 0x8);
    uint64_t taskinfo_offset = *(uint32_t *)(OS_SIZE_LOC - 0xC);
    taskinfo_offset = taskinfo_offset % (uint64_t)(0x200);
    task_num = *(uint16_t *)(OS_SIZE_LOC + 0x2);
    memcpy((uint8_t *)tasks, (uint8_t *)(0x52000000 + taskinfo_offset), taskinfo_size);
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
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
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // [p1-task2]: Interactive Character Input-Output System
    // int ch;
    // while ((ch=bios_getchar())) {
    //     if (ch != -1) {
    //         if (ch == '\r')
    //             bios_putstr("\n\r");
    //         else 
    //             bios_putchar(ch);
    //     }
    // }
    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    
    // [p1-task3]: 
    // int ch, taskid, buf_len = 0;
    // bios_putstr("$ ");
    // while ((ch=bios_getchar())) {
    //     if (ch == -1) continue;
    //     if (ch == '\r'){
    //         bios_putstr("\n\r");
    //         taskid = 0; int valid_input = 1;
    //         for (int i = 0; i < buf_len; ++i) {
    //             if (buf[i] < '0' || buf[i] > '9') {
    //                 valid_input = 0;
    //                 break;
    //             }
    //             taskid = taskid * 10 + buf[i] - '0';
    //         }
    //         if (taskid >= task_num)
    //             valid_input = 0;
    //         if (valid_input) {
    //             ((void (*)())load_task_img(taskid))();
    //         }
    //         else {
    //             bios_putstr("Invalid task id\n\r");
    //         }
    //         buf_len = 0;
    //         bios_putstr("$ ");
    //     }
    //     else {
    //         bios_putchar(ch);
    //         buf[buf_len++] = ch;
    //     }
    // }

    // [p1-task4]:
    int ch, buf_len = 0;
    memset(buf, 0, sizeof(buf));
    bios_putstr("$ ");
    while ((ch=bios_getchar())) {
        if (ch == -1) continue;
        if (ch == '\r') {
            bios_putstr("\n\r");
            int valid_input = from_name_load_task_img(buf);
            memset(buf, 0, sizeof(buf)); buf_len = 0;
            if (!valid_input) {
                bios_putstr("Invalid task name\n\r");
            }
            bios_putstr("$ ");
        }
        else {
            bios_putchar(ch);
            buf[buf_len++] = ch;
        }
    }
    
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
