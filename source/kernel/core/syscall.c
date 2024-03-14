#include "core/syscall.h"
#include "applib/lib_syscall.h"
#include "core/task.h"
#include "tools/log.h"

void sys_print_msg(const char *fmt, int arg) {
	log_printf(fmt, arg);
}

typedef int (*syscall_handler_t)(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
static const syscall_handler_t sys_table[] = {
	[SYS_sleep] = (syscall_handler_t) sys_sleep,
	[SYS_getpid] = (syscall_handler_t) sys_getpid,
	[SYS_print_msg] = (syscall_handler_t) sys_print_msg,
};

void do_handler_syscall(syscall_frame_t *frame) {
	if (frame->func_id < sizeof(sys_table) / sizeof(sys_table[0])) {
		syscall_handler_t handler = sys_table[frame->func_id];
		if (handler) {
			// log_printf("syscall %d, task: %s", frame->func_id, task_current()->name);
			frame->eax = handler(frame->arg0, frame->arg1, frame->arg2, frame->arg3);
			return;
		}
	}

	task_t *task = task_current();
	log_printf("syscall %d not found, task: %s", frame->func_id, task->name);
	frame->eax = -1;
}