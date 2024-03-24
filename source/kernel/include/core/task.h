#ifndef OS_TASK_H
#define OS_TASK_H

#include "comm/types.h"
#include "cpu/cpu.h"
#include "tools/list.h"
#include "fs/file.h"

#define TASK_NAME_SIZE              32
#define TASK_TIME_SLICE_DEFAULT     10
#define TASK_OFILE_NR               128
#define TASK_FLAG_SYSTEM            (1 << 0)

typedef struct _task_args_t {
	uint32_t return_addr;
	uint32_t argc;
	char **argv;
} task_args_t;

typedef struct _task_t {
	// uint32_t *stack;
	enum {
		TASK_CREATED,
		TASK_RUNNING,
		TASK_SLEEP,
		TASK_READY,
		TASK_WAITTING,
		TASK_ZOMBIE
	} state;

	int pid;
	struct _task_t *parent;
	uint32_t heap_start;
	uint32_t heap_end;

	int sleep_ticks;
	int time_ticks;
	int slice_ticks;
	int status;

	file_t *file_table[TASK_OFILE_NR];
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

file_t *task_file(int fd);
int task_alloc_fd(file_t *file);
void task_free_fd(int fd);

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
int sys_yield();
void sys_exit(int status);
int sys_wait(int *status);

void task_dispatch();
void task_time_tick();

void task_set_sleep(task_t *task, uint32_t ticks);
void task_set_wakeup(task_t *task);
void sys_sleep(uint32_t ms);
int sys_getpid();
int sys_fork();
int sys_execve(char *name, char **argv, char **env);

#endif //OS_TASK_H
