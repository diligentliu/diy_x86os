#ifndef OS_LIB_SYSCALL_H
#define OS_LIB_SYSCALL_H

#include "comm/types.h"
#include "core/syscall.h"
#include "os_cfg.h"
#include <sys/stat.h>

typedef struct _syscall_args_t {
	int id;
	int arg0;
	int arg1;
	int arg2;
	int arg3;
} syscall_args_t;

#define SYS_sleep 0

static inline int sys_call(syscall_args_t *args) {
	uint32_t addr[] = {0, SELECTOR_SYSCALL | 0};
	int ret;
	__asm__ __volatile__(
			"push %[arg3]\n\t"
			"push %[arg2]\n\t"
			"push %[arg1]\n\t"
			"push %[arg0]\n\t"
			"push %[id]\n\t"
			"lcall *(%[a])"
			:"=a"(ret):
	[arg3]"r"(args->arg3),
	[arg2]"r"(args->arg2),
	[arg1]"r"(args->arg1),
	[arg0]"r"(args->arg0),
	[id]"r"(args->id),
	[a]"r"(addr)
	);
	return ret;
}

void msleep(int ms);
int getpid();
void print_msg(const char *fmt, int arg);
int fork();
int execve(const char *path, char *const argv[], char *const envp[]);
void yield();

// 文件操作
int open(const char *name, int flags, ...);
int read(int file, char *ptr, int len);
int write(int file, char *ptr, int len);
int close(int file);
int lseek(int file, int ptr, int dir);

int isatty(int file);
int fstat(int file, struct stat *st);
void *sbrk(ptrdiff_t incr);
int dup(int fd);

#endif //OS_LIB_SYSCALL_H
