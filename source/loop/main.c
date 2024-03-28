#include "lib_syscall.h"
#include "main.h"
#include "fs/file.h"
#include "dev/tty.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/file.h>

int main(int argc, char *argv[]) {
	if (argc == 1) {
		char string_buf[128];
		fgets(string_buf, sizeof(string_buf), stdin);
		string_buf[sizeof(string_buf) - 1] = '\0';
		puts(string_buf);
		return 0;
	}

	int count = 1;
	char c;
	while ((c = getopt(argc, argv, "n:h")) != -1) {
		switch (c) {
			case 'h':
				puts("echo ns strings");
				puts("Usage: echo [-n cnt] string");
				optind = 1;        // getopt需要多次调用，需要重置
				return 0;
			case 'n':
				count = atoi(optarg);
				break;
			case '?':
				if (optarg) {
					fprintf(stderr, ESC_COLOR_ERROR"Unknown option: -%s\n"ESC_COLOR_DEFAULT, optarg);
				}
				optind = 1;
				return -1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, ESC_COLOR_ERROR"error: missing string\n"ESC_COLOR_DEFAULT);
		optind = 1;
		return -1;
	}

	char *string = argv[optind];
	for (int i = 0; i < count; i++) {
		printf("%s\n", string);
	}
	optind = 1;
	return 0;
}
