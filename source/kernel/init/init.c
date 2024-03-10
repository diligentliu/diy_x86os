/**
 * 内核初始化以及测试代码
 */
#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "core/task.h"
#include "tools/list.h"
#include "ipc/sem.h"
#include "kernel/include/core/memory.h"

static boot_info_t *init_boot_info;        // 启动信息

/**
 * 内核入口
 */
void kernel_init(boot_info_t *boot_info) {
	// ASSERT(3 < 2);
	init_boot_info = boot_info;

	// 初始化CPU，再重新加载
	cpu_init();

	memory_init(boot_info);
	log_init();
	irq_init();
	time_init();
	task_manager_init();
}

static task_t son_task;
static uint32_t son_task_stack[1024];
static sem_t sem;

void son_task_entry(void) {
	int count = 0;
	for (;;) {
		// sem_p(&sem);
		log_printf("son task: %d", count++);
		// task_switch_from_to(&son_task, task_init_task());
		// sys_sched_yield();
		sys_sleep(500);
	}
}

void list_test() {
	struct type_t {
		int i;
		list_node_t node;
	} v = {0x123456};
	list_node_t *v_node = &v.node;
	struct type_t *p = list_node_parent(v_node, struct type_t, node);
	if (p->i != 0x123456) {
		log_printf("list_node_parent error");
	} else {
		log_printf("list_node_parent success");
	}
}

void init_main(void) {
	log_printf("Kernel is running....");
	log_printf("Version: %s", OS_VERSION);

	list_test();
	
	// int a = 3 / 0;
	// irq_enable_global();

	task_init(&son_task, "son task", (uint32_t) son_task_entry, (uint32_t) &son_task_stack[1024]);
	init_task_init();

	// 初始化信号量 (要在中断开启之前)
	sem_init(&sem, 0);

	irq_enable_global();

	int count = 0;
	for (;;) {
		log_printf("init task: %d", count++);
		// task_switch_from_to(task_init_task(), &son_task);
		// sem_v(&sem);
		sys_sleep(1000);
	}
}
