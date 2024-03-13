#include "core/task.h"
#include "tools/log.h"

int first_task_main() {
	int count = 0;
	while (1) {
		// log_printf("first_task_main, %d", ++count);
		// sys_sleep(1000);
	}
	return 0;
}