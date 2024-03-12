#include "core/task.h"
#include "tools/klib.h"
#include "cpu/cpu.h"
#include "os_cfg.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "cpu/irq.h"
#include "cpu/mmu.h"
#include "core/memory.h"

static task_manager_t task_manager;
static uint32_t idle_task_stack[IDLE_TASK_STACK_SIZE];

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
	task->tss.eflags = EFLAGS_IF | EFLAGS_DEFAULT;
	task->tss.iomap = 0;

	// 页表初始化
	uint32_t page_dir = memory_create_uvm();
	if (page_dir == 0) {
		gdt_free_sel(tss_selector);
		return -1;
	}
	task->tss.cr3 = page_dir;

	task->tss_selector = tss_selector;
	return 0;
}

int task_init(task_t *task, const char *name, uint32_t entry, uint32_t esp) {
	ASSERT(task != (task_t *) (0));

	tss_init(task, entry, esp);

	kernel_strncpy(task->name, name, TASK_NAME_SIZE);
	task->state = TASK_CREATED;
	task->sleep_ticks = 0;
	task->time_ticks = TASK_TIME_SLICE_DEFAULT;
	task->slice_ticks = TASK_TIME_SLICE_DEFAULT;
	list_node_init(&task->run_node);
	list_node_init(&task->wait_node);
	list_node_init(&task->all_node);

	irq_state_t state = irq_enter_protection();
	task_set_ready(task);
	list_push_back(&task_manager.task_list, &task->all_node);
	irq_leave_protection(state);
	return 0;
}

void task_switch_from_to(task_t *from, task_t *to) {
	switch_to_tss(to->tss_selector);
	// simple_switch(&from->stack, to->stack);
}

static void idle_task_entry() {
	while (1) {
		hlt();
	}
}

void task_manager_init() {
	task_manager.current = (task_t *) 0;
	list_init(&task_manager.sleep_list);
	list_init(&task_manager.ready_list);
	list_init(&task_manager.task_list);

	task_init(&task_manager.idle_task, "idle",
	          (uint32_t) idle_task_entry,
	          (uint32_t) &idle_task_stack[IDLE_TASK_STACK_SIZE]);
}

void first_task_init() {
	void first_task_entry();
	extern uint8_t s_first_task[], e_first_task[];

	uint32_t copy_size = (uint32_t)(e_first_task - s_first_task);
	uint32_t alloc_size = 10 * MEM_PAGE_SIZE;
	ASSERT(copy_size < alloc_size);

	uint32_t first_start = (uint32_t) first_task_entry;

	task_init(&task_manager.first_task, "first task", first_start, 0);
	task_manager.current = &task_manager.first_task;

	mmu_set_page_dir(task_manager.first_task.tss.cr3);

	memory_alloc_page_for(first_start, alloc_size, PTE_P | PTE_W);
	kernel_memcpy((void *) first_start, (void *) s_first_task, copy_size);
	
	write_tr(task_manager.first_task.tss_selector);
}

task_t *task_first_task() {
	return &task_manager.first_task;
}

void task_set_ready(task_t *task) {
	if (task == &task_manager.idle_task) {
		return;
	}
	list_push_back(&task_manager.ready_list, &task->run_node);
	task->state = TASK_READY;
}

void task_set_block(task_t *task) {
	if (task == &task_manager.idle_task) {
		return;
	}
	list_ease(&task_manager.ready_list, &task->run_node);
}

task_t *task_current() {
	return task_manager.current;
}

// 返回下一个要运行的任务 (待优化)
task_t *task_next_run() {
	if (list_is_empty(&task_manager.ready_list)) {
		return &task_manager.idle_task;
	}
	list_node_t *task_node = list_first(&task_manager.ready_list);
	return list_node_parent(task_node, task_t, run_node);
}

int sys_sched_yield() {
	irq_state_t state = irq_enter_protection();

	if (list_is_empty(&task_manager.ready_list)) {
		return -1;
	} else if (list_count(&task_manager.ready_list) == 1) {
		return 0;
	}

	task_t *current = task_current();
	task_set_block(current);
	task_set_ready(current);

	task_dispatch();

	irq_leave_protection(state);
	return 0;
}

void task_dispatch() {
	irq_state_t state = irq_enter_protection();

	task_t *to = task_next_run();
	if (to == (task_t *) 0 || to == task_current()) {
		return;
	}
	task_t *from = task_current();
	task_manager.current = to;
	to->state = TASK_RUNNING;
	task_switch_from_to(from, to);

	irq_leave_protection(state);
}

void task_time_tick() {
	task_t *current = task_current();
	if (--current->slice_ticks <= 0) {
		current->slice_ticks = current->time_ticks;
		task_set_block(current);
		task_set_ready(current);
		task_dispatch();
	}

	list_node_t *node = list_first(&task_manager.sleep_list);
	while (node) {
		task_t *task = list_node_parent(node, task_t, run_node);
		list_node_t *next = list_node_next(node);
		if (--task->sleep_ticks <= 0) {
			task_set_wakeup(task);
			task_set_ready(task);
		}
		node = next;
	}
}

void task_set_sleep(task_t *task, uint32_t ticks) {
	if (ticks == 0) {
		return;
	}
	task->sleep_ticks = ticks;
	task->state = TASK_SLEEP;
	list_push_back(&task_manager.sleep_list, &task->run_node);
}

void task_set_wakeup(task_t *task) {
	list_ease(&task_manager.sleep_list, &task->run_node);
}

void sys_sleep(uint32_t ms) {
	irq_state_t state = irq_enter_protection();
	task_t *current = task_current();
	task_set_block(current);
	task_set_sleep(current, (ms + (OS_TICKS_MS - 1)) / OS_TICKS_MS);
	task_dispatch();
	irq_leave_protection(state);
}