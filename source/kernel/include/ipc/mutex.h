#ifndef OS_MUTEX_H
#define OS_MUTEX_H

#include "tools/list.h"
#include "core/task.h"

typedef struct _mutex_t {
	task_t *owner;
	int locked_count;       // 锁定次数
	list_t wait_list;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif //OS_MUTEX_H
