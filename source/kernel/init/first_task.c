#include "core/task.h"
#include "tools/log.h"
#include "applib/lib_syscall.h"
#include "dev/tty.h"

int first_task_main() {
#if 0
	print_msg("first_task_main, pid: %d\n", getpid());
	int count = 0;
	int pid = fork();
	if (pid == 0) {
		++count;
		print_msg("child task, pid: %d\n", getpid());
		print_msg("count = %d\n", count);

		char *argv[] = {"arg0", "arg1", "arg2", "arg3"};
		execve("/shell.elf", argv, (char **) 0);
	} else if (pid > 0) {
		++count;
		print_msg("parent task, pid: %d\n", getpid());
		print_msg("count = %d\n", count);
	} else {
		print_msg("fork failed\n", 0);
	}
#endif

	for (int i = 0; i < TTY_NR; ++i) {
		int pid = fork();
		if (pid < 0) {
			print_msg("fork failed\n", 0);
			break;
		} else if (pid == 0) {
			char tty_num[] = "tty:?";
			tty_num[4] = '0' + i;
			char *argv[] = {tty_num, (char *) 0};
			execve("/shell.elf", argv, (char **) 0);
			while (1) {
				msleep(10000);
			}
		}
	}

	while (1) {
		// print_msg("first_task_main, %d\n", ++count);
		msleep(10000);
	}
	return 0;
}