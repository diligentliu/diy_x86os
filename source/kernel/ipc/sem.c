#include "ipc/sem.h"
#include "core/task.h"
#include "cpu/irq.h"

/**
 * 初始化信号量
 */
void sem_init(sem_t *sem, int init_count) {
	sem->count = init_count;
	list_init(&sem->wait_list);
}

/**
 * 信号量 P 操作
 */
void sem_p(sem_t *sem) {
	irq_state_t state = irq_enter_protection();

	if (sem->count > 0) {
		sem->count--;
	} else {
		task_t *current = task_current();
		task_set_block(current);
		list_push_back(&sem->wait_list, &current->wait_node);
		task_dispatch();
	}

	irq_leave_protection(state);
}

/**
 * 信号量 V 操作
 */
void sem_v(sem_t *sem) {
	irq_state_t state = irq_enter_protection();

	if (list_is_empty(&sem->wait_list)) {
		sem->count++;
	} else {
		list_node_t *node = list_pop_front(&sem->wait_list);
		task_t *task = list_node_parent(node, task_t, wait_node);
		task_set_ready(task);
		task_dispatch();
	}

	irq_leave_protection(state);
}

/**
 * 获取信号量计数
 */
int sem_count(sem_t *sem) {
	irq_state_t state = irq_enter_protection();
	int count = sem->count;
	irq_leave_protection(state);

	return count;
}