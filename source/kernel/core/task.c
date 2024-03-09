#include "core/task.h"
#include "tools/klib.h"
#include "cpu/cpu.h"
#include "os_cfg.h"
#include "tools/log.h"

static int tss_init(task_t *task, uint32_t entry, uint32_t esp) {
	int tss_selector = gdt_alloc_desc();
	if (tss_selector < 0) {
		log_printf("alloc tss selector failed");
		return -1;
	}

	segment_desc_set(tss_selector, (uint32_t) &task->tss, sizeof(tss_t),
					 SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);
	kernel_memset(&task->tss, 0, sizeof(tss_t));
	task->tss.eip = entry;
	task->tss.esp = task->tss.esp0 = esp;
	task->tss.ss = task->tss.ss0 = KERNEL_SELECTOR_DS;
	task->tss.es = task->tss.fs = task->tss.gs = task->tss.ds = KERNEL_SELECTOR_DS;
	task->tss.cs = KERNEL_SELECTOR_CS;
	task->tss.eflags = EFALGS_IF | EFLAGS_DEFAULT;
	task->tss_selector = tss_selector;
	return 0;
}

int task_init(task_t *task, uint32_t entry, uint32_t esp) {
	ASSERT(task != (task_t *)(0));

	// tss_init(task, entry, esp);
	uint32_t *pesp = (uint32_t *) esp;
	if (pesp) {
		*(--pesp) = entry;
		*(--pesp) = 0;
		*(--pesp) = 0;
		*(--pesp) = 0;
		*(--pesp) = 0;
		task->stack = pesp;
		return 0;
	} else {
		log_printf("task_init: esp is null");
		return -1;
	}
}

void task_switch_from_to(task_t *from, task_t *to) {
	// switch_to_tss(to->tss_selector);
	simple_switch(&from->stack, to->stack);
}