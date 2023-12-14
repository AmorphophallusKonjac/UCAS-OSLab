#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE 0x52000000
#define TASK_MAXNUM 30
#define TASK_SIZE 0x10000

#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) \
	(((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
	uint32_t offset;
	uint32_t size;
	char name[32];
	uint64_t entrypoint;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

extern uint16_t task_num;

#endif