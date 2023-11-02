#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

uint64_t load_task_img(int taskid);
int from_name_load_task_img(char *name);
uint64_t getEntrypoint(char *name);

#endif