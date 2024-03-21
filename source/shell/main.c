#include "lib_syscall.h"
#include <stdio.h>

int main(int argc, char *argv[]) {

	int count = 0;

	printf("Hello, Shell!\n");
	printf("\0337Hello, World!\0338123\n");
	printf("\033[31;42mHello, World!\033[0m123\n");
	printf("123\033[2DHello, World!\n");    // 1Hello, World!
	printf("123\033[2CHello, World!\n");    // 123  Hello, World!

	printf("\033[31m\n");
	printf("\033[10;10H test!\n");
	printf("\033[20;20H test!\n");
	printf("\033[32;25;39m123\n");

	printf("\033[2J\n");

	for (int i = 0; i < argc; ++i) {
		printf("argv = %s\n", argv[i]);
	}

	fork();
	yield();

	while (1) {
		printf("shell pid = %d\n", getpid());
		printf("shell count = %d\n", count++);
		msleep(1000);
	}
	return 0;
}