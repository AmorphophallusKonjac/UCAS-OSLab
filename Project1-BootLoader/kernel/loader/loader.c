#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    // [p1-task3]
    // bios_sd_read(TASK_MEM_BASE + TASK_SIZE * taskid, 15, 1 + 15 * (taskid + 1));
    // return TASK_MEM_BASE + TASK_SIZE * taskid;

    // [p1-task4]
    uint32_t task_sector_id = tasks[taskid].offset / SECTOR_SIZE;
    uint32_t task_sector_num = (tasks[taskid].offset + tasks[taskid].size) / SECTOR_SIZE - task_sector_id + 1;
    bios_sd_read(tasks[taskid].entrypoint, task_sector_num, task_sector_id);
    memcpy((uint8_t *)(tasks[taskid].entrypoint), (uint8_t *)(tasks[taskid].entrypoint + (tasks[taskid].offset % SECTOR_SIZE)), tasks[taskid].size);
    return tasks[taskid].entrypoint;
}

int from_name_load_task_img(char* name) {
    for (int taskidx = 0; taskidx < task_num; ++taskidx) {
        if (strcmp(tasks[taskidx].name, name) == 0) {
            ((void (*)())load_task_img(taskidx))();
            return 1;
        }
    }
    return 0;
}

