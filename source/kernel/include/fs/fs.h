#ifndef OS_FS_H
#define OS_FS_H

#include <sys/stat.h>

int sys_open(const char *path, int flags, ...);
int sys_read(int fd, void *buf, int len);
int sys_write(int fd, char *buf, int len);
int sys_lseek(int fd, int offset, int whence);
int sys_close(int fd);

int sys_isatty(int fd);
int sys_fstat(int fd, struct stat *st);

#endif //OS_FS_H
