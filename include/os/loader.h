#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <os/page.h>
#include <os/sched.h>

uint64_t load_task_img(int taskid);
uint64_t from_name_load_task_img(char *name, pcb_t *pcb);
uint64_t getEntrypoint(char *name);

#endif