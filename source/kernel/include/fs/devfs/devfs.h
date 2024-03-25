#ifndef OS_DEVFS_H
#define OS_DEVFS_H

#include "fs/fs.h"
#include "fs/file.h"

typedef struct _devfs_type_t {
	const char *name;
	int dev_type;
	int file_type;
} devfs_type_t;

#endif //OS_DEVFS_H
