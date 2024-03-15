#ifndef OS_LIB_SYSCALL_H
#define OS_LIB_SYSCALL_H

#include "comm/types.h"
#include "core/syscall.h"
#include "os_cfg.h"

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

static inline void msleep(int ms) {
	if (ms <= 0) {
		return;
	}
	syscall_args_t args = {SYS_sleep, ms, 0, 0, 0};
	sys_call(&args);
}

static inline int getpid() {
	syscall_args_t args = {SYS_getpid, 0, 0, 0, 0};
	return sys_call(&args);
}

static inline void print_msg(const char *fmt, int arg) {
	syscall_args_t args = {SYS_print_msg, (uint32_t) fmt, arg, 0, 0};
	sys_call(&args);
}

static inline int fork() {
	syscall_args_t args = {SYS_fork, 0, 0, 0, 0};
	return sys_call(&args);
}

static inline int execve(const char *path, char *const argv[], char *const envp[]) {
	syscall_args_t args = {SYS_execve, (uint32_t) path, (uint32_t) argv, (uint32_t) envp, 0};
	return sys_call(&args);
}

#endif //OS_LIB_SYSCALL_H
