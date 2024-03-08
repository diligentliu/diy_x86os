/**
 * 内核初始化以及测试代码
 */
#include "comm/boot_info.h"
#include "comm/cpu_instr.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "core/task.h"

static boot_info_t *init_boot_info;        // 启动信息

/**
 * 内核入口
 */
void kernel_init(boot_info_t *boot_info) {
	// ASSERT(3 < 2);
	init_boot_info = boot_info;

	// 初始化CPU，再重新加载
	cpu_init();

	log_init();
	irq_init();
	time_init();
}

static task_t first_task;
static task_t second_task;
static uint32_t second_task_stack[1024];

void init_task_entry(void) {
	int count = 0;
	for (;;) {
		log_printf("second task: %d", count++);
		task_switch_from_to(&second_task, &first_task);
	}
}

void init_main(void) {
	log_printf("Kernel is running....");
	log_printf("Version: %s", OS_VERSION);

	// int a = 3 / 0;
	// irq_enable_global();
	task_init(&first_task, 0, 0);
	write_tr(first_task.tss_selector);

	task_init(&second_task, (uint32_t)init_task_entry, (uint32_t) &second_task_stack[1024]);
	int count = 0;
	for (;;) {
		log_printf("first main: %d", count++);
		task_switch_from_to(&first_task, &second_task);
	}
}
