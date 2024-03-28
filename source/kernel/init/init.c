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
#include "core/memory.h"
#include "tools/klib.h"
#include "dev/console.h"
#include "dev/keyboard.h"
#include "fs/fs.h"

/**
 * 内核入口
 */
void kernel_init(boot_info_t *boot_info) {
	// 初始化CPU，再重新加载
	cpu_init();
	irq_init();

	log_init();
	memory_init(boot_info);
	fs_init();
	time_init();
	task_manager_init();
}

void move_to_first_task(void) {
	task_t *current_task = task_current();
	ASSERT(current_task != 0);

	tss_t *tss = &current_task->tss;
	__asm__ __volatile__ (
			"push %[ss]\n\t"
			"push %[esp]\n\t"
			"push %[eflags]\n\t"
			"push %[cs]\n\t"
			"push %[eip]\n\t"
			"iret"
			::[ss]"r"(tss->ss),
	[esp]"r"(tss->esp),
	[eflags]"r"(tss->eflags),
	[cs]"r"(tss->cs),
	[eip]"r"(tss->eip)
	);
}

void init_main(void) {
	log_printf("=========================\n");
	log_printf("Kernel is running....\n");
	log_printf("Version: %s\n", OS_VERSION);
	log_printf("=========================\n");

	// int a = 3 / 0;
	// irq_enable_global();

	first_task_init();
	move_to_first_task();
}
