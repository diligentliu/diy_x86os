#include "lib_syscall.h"
#include "malloc.h"

void msleep(int ms) {
	if (ms <= 0) {
		return;
	}
	syscall_args_t args = {SYS_sleep, ms, 0, 0, 0};
	sys_call(&args);
}

int getpid() {
	syscall_args_t args = {SYS_getpid, 0, 0, 0, 0};
	return sys_call(&args);
}

void print_msg(const char *fmt, int arg) {
	syscall_args_t args = {SYS_print_msg, (uint32_t) fmt, arg, 0, 0};
	sys_call(&args);
}

int fork() {
	syscall_args_t args = {SYS_fork, 0, 0, 0, 0};
	return sys_call(&args);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
	syscall_args_t args = {SYS_execve, (uint32_t) path, (uint32_t) argv, (uint32_t) envp, 0};
	return sys_call(&args);
}

void yield() {
	syscall_args_t args = {SYS_yield, 0, 0, 0, 0};
	sys_call(&args);
}

void _exit(int status) {
	syscall_args_t args;
	args.id = SYS_exit;
	args.arg0 = status;
	sys_call(&args);
	while (1);
}

int wait(int *status) {
	syscall_args_t args;
	args.id = SYS_wait;
	args.arg0 = (int) status;
	return sys_call(&args);
}

int open(const char *name, int flags, ...) {
	// 不考虑支持太多参数
	syscall_args_t args;
	args.id = SYS_open;
	args.arg0 = (int) name;
	args.arg1 = (int) flags;
	return sys_call(&args);
}

int read(int file, char *ptr, int len) {
	syscall_args_t args;
	args.id = SYS_read;
	args.arg0 = (int) file;
	args.arg1 = (int) ptr;
	args.arg2 = len;
	return sys_call(&args);
}

int write(int file, char *ptr, int len) {
	syscall_args_t args;
	args.id = SYS_write;
	args.arg0 = (int) file;
	args.arg1 = (int) ptr;
	args.arg2 = len;
	return sys_call(&args);
}

int close(int file) {
	syscall_args_t args;
	args.id = SYS_close;
	args.arg0 = (int) file;
	return sys_call(&args);
}

int lseek(int file, int ptr, int dir) {
	syscall_args_t args;
	args.id = SYS_lseek;
	args.arg0 = (int) file;
	args.arg1 = (int) ptr;
	args.arg2 = dir;
	return sys_call(&args);
}

int ioctl(int file, int cmd, int arg0, int arg1) {
	syscall_args_t args;
	args.id = SYS_ioctl;
	args.arg0 = (int) file;
	args.arg1 = cmd;
	args.arg2 = arg0;
	args.arg3 = arg1;
	return sys_call(&args);
}

/**
 * 获取文件的状态
 */
int fstat(int file, struct stat *st) {
    syscall_args_t args;
    args.id = SYS_fstat;
    args.arg0 = (int) file;
    args.arg1 = (int) st;
    return sys_call(&args);
}

int unlink(const char *name) {
	syscall_args_t args;
	args.id = SYS_unlink;
	args.arg0 = (int) name;
	return sys_call(&args);
}

/**
 * 判断文件描述符与tty关联
 */
int isatty(int file) {
    syscall_args_t args;
    args.id = SYS_isatty;
    args.arg0 = (int) file;
    return sys_call(&args);
}

void * sbrk(ptrdiff_t incr) {
    syscall_args_t args;
    args.id = SYS_sbrk;
    args.arg0 = (int) incr;
    return (void *)sys_call(&args);
}

int dup(int fd) {
	syscall_args_t args;
	args.id = SYS_dup;
	args.arg0 = (int) fd;
	return sys_call(&args);
}

DIR *opendir(const char *path) {
	DIR *dir = (DIR *) malloc(sizeof(DIR));
	if (dir == (DIR *) 0) {
		return (DIR *) 0;
	}

	syscall_args_t args;
	args.id = SYS_opendir;
	args.arg0 = (int) path;
	args.arg1 = (int) dir;
	int err = sys_call(&args);
	if (err < 0) {
		free(dir);
		return (DIR *) 0;
	}

	return dir;
}

struct dirent *readdir(DIR *dir) {
	syscall_args_t args;
	args.id = SYS_readdir;
	args.arg0 = (int) dir;
	args.arg1 = (int) &dir->dirent;
	int err = sys_call(&args);
	if (err < 0) {
		return (struct dirent *) 0;
	}

	return &dir->dirent;
}

int closedir(DIR *dir) {
	syscall_args_t args;
	args.id = SYS_closedir;
	args.arg0 = (int) dir;
	sys_call(&args);
	free(dir);
	return 0;
}