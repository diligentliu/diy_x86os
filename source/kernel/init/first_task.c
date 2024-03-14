#include "core/task.h"
#include "tools/log.h"
#include "applib/lib_syscall.h"

int first_task_main() {
	print_msg("first_task_main, pid: %d", getpid());
	int count = 0;
	int pid = fork();
	if (pid == 0) {
		++count;
		print_msg("child task, pid: %d", getpid());
		print_msg("count = %d", count);
	} else if (pid > 0) {
		++count;
		print_msg("parent task, pid: %d", getpid());
		print_msg("count = %d", count);
	} else {
		print_msg("fork failed", 0);
	}

	while (1) {
		print_msg("first_task_main, %d", ++count);
		msleep(1000);
	}
	return 0;
}