#include "lib_syscall.h"
#include "main.h"
#include "fs/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/file.h>

static cli_t cli;
static const char *promot = "sh >> ";

static void cli_init(const char *promot, const cli_cmd_t *cmd_list, int size) {
	cli.promot = promot;
	memset(cli.curr_input, 0, CLI_INPUT_SIZE);
	cli.cmd_start = cmd_list;
	cli.cmd_end = cmd_list + size;
}

static void show_promot() {
	fflush(stdout);
	printf("%s", cli.promot);
	fflush(stdout);
}

static int do_help(int argc, char *argv[]) {
	if (argc > 1) {
		printf(ESC_COLOR_ERROR"help: excess parameters"ESC_COLOR_DEFAULT);
		return -1;
	}
	const cli_cmd_t *start = cli.cmd_start;
	while (start < cli.cmd_end) {
		printf("%s\n    %s\n", start->name, start->usage);
		++start;
	}
	return 0;
}

static int do_echo(int argc, char *argv[]) {
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

static int do_clear(int argc, char *argv[]) {
	if (argc > 1) {
		printf(ESC_COLOR_ERROR"clear: excess parameters"ESC_COLOR_DEFAULT);
		return -1;
	}
	printf("%s", ESC_CLEAR_SCREEN);
	printf("%s", ESC_MOVE_CURSOR(0, 0));
	return 0;
}

static int do_ls(int argc, char *argv[]) {
	if (argc > 1) {
		printf(ESC_COLOR_ERROR"ls: excess parameters"ESC_COLOR_DEFAULT);
		return -1;
	}
	DIR *dir = opendir(".");
	if (dir == (DIR *) 0) {
		fprintf(stderr, ESC_COLOR_ERROR"ls: opendir failed\n"ESC_COLOR_DEFAULT);
		return -1;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != (struct dirent *) 0) {
		strlwr(entry->name);
		printf("%c  %s  %d\n",
			   entry->type == FILE_TYPE_DIR ? 'd' : 'f',
			   entry->name,
			   entry->size
	    );
	}
	return 0;
}

static int do_exit(int argc, char *argv[]) {
	if (argc > 1) {
		printf(ESC_COLOR_ERROR"exit: excess parameters"ESC_COLOR_DEFAULT);
		return -1;
	}
	exit(0);
}

static const cli_cmd_t cmd_list[] = {
		{
			.name = "help",
			.usage = "help -- list supported command",
			.do_func = do_help
		},
		{
			.name = "clear",
			.usage = "clear -- clear screen",
			.do_func = do_clear
		},
		{
			.name = "echo",
			.usage = "echo [-n cnt] string -- echo ns strings",
			.do_func = do_echo
		},
		{
				.name = "ls",
				.usage = "ls -- list files",
				.do_func = do_ls
		},
		{
			.name = "exit",
			.usage = "exit -- exit shell",
			.do_func = do_exit
		},
};

static const cli_cmd_t *find_builtin(const char *name) {
	for (const cli_cmd_t * cmd = cli.cmd_start; cmd < cli.cmd_end; cmd++) {
		if (strcmp(cmd->name, name) != 0) {
			continue;
		}
		return cmd;
	}
	return (const cli_cmd_t *) 0;
}

static void run_builtin(const cli_cmd_t *cmd, int argc, char *argv[]) {
	int ret = cmd->do_func(argc, argv);
	if (ret < 0) {
		fprintf(stderr,ESC_COLOR_ERROR"error: %d\n"ESC_COLOR_DEFAULT, ret);
	}
}

static void run_exec_file(const char *name, int argc, char *argv[]) {
	int pid = fork();
	if (pid < 0) {
		fprintf(stderr, ESC_COLOR_ERROR"fork failed\n"ESC_COLOR_DEFAULT);
		return;
	} else if (pid == 0) {
		// execve(name, argv, NULL);
		for (int i = 0; i < argc; ++i) {
			msleep(1000);
			printf("arg %d = %s\n", i, argv[i]);
		}
		// fprintf(stderr, ESC_COLOR_ERROR"execve: %s failed\n"ESC_COLOR_DEFAULT, name);
		exit(-1);
	} else {
		int status;
		int pid = wait(&status);
		fprintf(stderr, "cmd %s, pid = %d exit with status %d\n", name, pid, status);
	}
}

int main(int argc, char *argv[]) {
	int fd_stdin = open(argv[0], O_RDWR);
	int fd_stdout = dup(fd_stdin);
	int fd_stderr = dup(fd_stdin);

	cli_init(promot, cmd_list, sizeof(cmd_list) / sizeof(cmd_list[0]));

	while (1) {
		show_promot();
		char *str = fgets(cli.curr_input, CLI_INPUT_SIZE, stdin);
		char * cr = strchr(cli.curr_input, '\n');
		if (cr) {
			*cr = '\0';
		}
		cr = strchr(cli.curr_input, '\r');
		if (cr) {
			*cr = '\0';
		}

		int argc_ = 0;
		char * argv_[CLI_MAX_ARG_COUNT];
		memset(argv_, 0, sizeof(argv_));

		const char *space = " ";  // 字符分割器
		char *token = strtok(cli.curr_input, space);
		while (token) {
			// 记录参数
			argv_[argc_++] = token;

			// 先获取下一位置
			token = strtok(NULL, space);
		}

		if (argc_ == 0) {
			continue;
		}

		const cli_cmd_t * cmd = find_builtin(argv_[0]);
		if (cmd) {
			run_builtin(cmd, argc_, argv_);
			continue;
		}

		run_exec_file(argv_[0], argc_, argv_);

		fprintf(stderr, ESC_COLOR_ERROR"command not found: %s\n"ESC_COLOR_DEFAULT, argv_[0]);
	}
}