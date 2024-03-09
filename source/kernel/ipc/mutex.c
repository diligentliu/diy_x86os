#include "ipc/mutex.h"
#include "cpu/irq.h"

/**
 * 初始化互斥锁
 */
void mutex_init(mutex_t *mutex) {
	mutex->owner = (task_t *) 0;
	mutex->locked_count = 0;
	list_init(&mutex->wait_list);
}

/**
 * 互斥锁加锁
 */
void mutex_lock(mutex_t *mutex) {
	irq_state_t state = irq_enter_protection();

	task_t *current = task_current();
	if (mutex->locked_count == 0) {
		mutex->locked_count++;
		mutex->owner = current;
	} else if (mutex->owner == current) {
		mutex->locked_count++;
	} else {
		task_set_block(current);
		list_push_back(&mutex->wait_list, &current->wait_node);
		task_dispatch();
	}

	irq_leave_protection(state);
}

/**
 * 互斥锁解锁
 */
void mutex_unlock(mutex_t *mutex) {
	irq_state_t state = irq_enter_protection();

	task_t *current = task_current();
	if (mutex->owner != current) {
		irq_leave_protection(state);
		return;
	}

	if (--mutex->locked_count == 0) {
		mutex->owner = (task_t *) 0;
		if (!list_is_empty(&mutex->wait_list)) {
			list_node_t *node = list_pop_front(&mutex->wait_list);
			task_t *task = list_node_parent(node, task_t, wait_node);
			mutex->locked_count++;
			mutex->owner = task;
			task_set_ready(task);
			task_dispatch();
		}
	}

	irq_leave_protection(state);
}