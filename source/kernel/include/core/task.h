#ifndef OS_TASK_H
#define OS_TASK_H

#include "comm/types.h"
#include "cpu/cpu.h"

typedef struct _task_t {
	uint32_t *stack;
	tss_t tss;
	int tss_selector;
} task_t;

int task_init(task_t *task, uint32_t entry, uint32_t esp);
void simple_switch(uint32_t *from, uint32_t *to);
void task_switch_from_to(task_t *from, task_t *to);

#endif //OS_TASK_H
