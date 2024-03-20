#include "lib_syscall.h"
#include <stdio.h>

int main(int argc, char *argv[]) {

	int count = 0;

	for (int i = 0; i < argc; ++i) {
		printf("argv = %s", argv[i]);
	}

	fork();
	yield();

	while (1) {
		printf("shell pid = %d", getpid());
		printf("shell count = %d", count++);
		msleep(1000);
	}
	return 0;
}