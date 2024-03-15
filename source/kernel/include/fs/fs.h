#ifndef OS_FS_H
#define OS_FS_H

int sys_open(const char *path, int flags, ...);
int sys_read(int fd, void *buf, int len);
int sys_write(int fd, const void *buf, int len);
int sys_lseek(int fd, int offset, int whence);
int sys_close(int fd);

#endif //OS_FS_H
