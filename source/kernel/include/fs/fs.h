#ifndef OS_FS_H
#define OS_FS_H

#include <sys/stat.h>
#include "file.h"
#include "tools/list.h"
#include "ipc/mutex.h"

struct _fs_t;

typedef struct _fs_op_t {
	int (*mount)(struct _fs_t *fs, int major, int minor);
	void (*unmount)(struct _fs_t *fs);
	int (*open)(struct _fs_t *fs, const char *path, file_t *file);
	int (*read)(void *buf, int len, file_t *file);
	int (*write)(char *buf, int len, file_t *file);
	void (*close)(file_t *file);
	int (*seek)(file_t *file, int offset, int whence);
	int (*stat)(file_t *file, struct stat *st);
} fs_op_t;

#define FS_MOUNT_POINT_LEN      512

typedef enum _fs_type_t {
	FS_TYPE_DEV = 0,
} fs_type_t;

typedef struct _fs_t {
	char mount_point[FS_MOUNT_POINT_LEN];
	fs_type_t type;
	fs_op_t *op;
	void *data;
	int dev_id;
	list_node_t node;
	mutex_t *mutex;
} fs_t;

void fs_init();

int path_to_num(const char *path, int *num);
int path_begin_with(const char *path, const char *str);
const char *path_next_child(const char *path);

int sys_open(const char *path, int flags, ...);
int sys_read(int fd, void *buf, int len);
int sys_write(int fd, char *buf, int len);
int sys_lseek(int fd, int offset, int whence);
int sys_close(int fd);

int sys_isatty(int fd);
int sys_fstat(int fd, struct stat *st);

int sys_dup(int fd);

#endif //OS_FS_H
