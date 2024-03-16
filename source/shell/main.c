#include "lib_syscall.h"
// #include <stdio.h>

int main(int argc, char *argv[]) {

	int count = 0;

	for (int i = 0; i < argc; ++i) {
		print_msg("argv = %s", (int) argv[i]);
	}

	fork();
	yield();

	while (1) {
		print_msg("shell pid = %d", getpid());
		print_msg("shell count = %d", count++);
		msleep(1000);
	}
	return 0;
}