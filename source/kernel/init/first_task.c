#include "core/task.h"
#include "tools/log.h"
#include "applib/lib_syscall.h"

int first_task_main() {
	int pid = getpid();
	int count = 0;
	print_msg("first_task_main, pid: %d", pid);
	while (1) {
		print_msg("first_task_main, %d", ++count);
		msleep(1000);
	}
	return 0;
}