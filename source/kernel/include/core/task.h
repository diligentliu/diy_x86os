#ifndef OS_TASK_H
#define OS_TASK_H

#include "comm/types.h"
#include "cpu/cpu.h"
#include "tools/list.h"

#define TASK_NAME_SIZE 32
#define TASK_TIME_SLICE_DEFAULT 10

#define TASK_FLAG_SYSTEM     (1 << 0)

typedef struct _task_t {
	// uint32_t *stack;
	enum {
		TASK_CREATED,
		TASK_RUNNING,
		TASK_SLEEP,
		TASK_READY,
		TASK_WAITTING,
	} state;

	int pid;

	int sleep_ticks;
	int time_ticks;
	int slice_ticks;

	char name[TASK_NAME_SIZE];
	list_node_t run_node;
	list_node_t wait_node;
	list_node_t all_node;
	tss_t tss;
	int tss_selector;
} task_t;

int task_init(task_t *task, const char *name, int flag, uint32_t entry, uint32_t esp);
void task_switch_from_to(task_t *from, task_t *to);
// 定义在汇编文件中
void simple_switch(uint32_t *from, uint32_t *to);

// 任务管理器
typedef struct _task_manager_t {
	task_t *current;        // 当前任务
	list_t ready_list;      // 就绪任务
	list_t task_list;       // 所有任务
	list_t sleep_list;      // 睡眠任务
	task_t first_task;       // 初始化任务
	task_t idle_task;       // 空闲任务

	int app_code_selector;
	int app_data_selector;
} task_manager_t;

void task_manager_init();
void first_task_init();
task_t *task_first_task();
void task_set_ready(task_t *task);
void task_set_block(task_t *task);
task_t *task_current();
int sys_sched_yield();
void task_dispatch();
void task_time_tick();

void task_set_sleep(task_t *task, uint32_t ticks);
void task_set_wakeup(task_t *task);
void sys_sleep(uint32_t ms);
uint32_t sys_getpid();

#endif //OS_TASK_H
