#ifndef OS_SEM_H
#define OS_SEM_H

#include "tools/list.h"

typedef struct _sem_t {
	int count;
	list_t wait_list;
} sem_t;

void sem_init(sem_t *sem, int init_count);
void sem_p(sem_t *sem);
void sem_v(sem_t *sem);
int sem_count(sem_t *sem);

#endif //OS_SEM_H
