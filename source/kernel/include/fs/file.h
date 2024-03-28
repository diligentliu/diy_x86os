#ifndef OS_FILE_H
#define OS_FILE_H

#include "comm/types.h"

#define FILE_TABLE_SIZE     2048
#define FILE_NAME_SIZE      32

typedef enum _file_type_t {
	FILE_TYPE_UNKNOWN = 0,
	FILE_TYPE_TTY,
	FILE_TYPE_DIR,
	FILE_TYPE_NORMAL,
} file_type_t;

struct _fs_t;
typedef struct _file_t {
	char name[FILE_NAME_SIZE];
	file_type_t type;
	uint32_t size;
	int ref;                    // 引用计数
	int dev_id;                 // 文件所属设备 id
	int pos;                    // 当前位置
	int mode;                   // 读写模式
	int sblk;                   // 起始块
	int cblk;                   // 当前块
	int index;                  // 在父目录表项的文件索引
	struct _fs_t *fs;
} file_t;

file_t *file_alloc();
void file_free(file_t *file);
void file_table_init();
void file_inc_ref(file_t *file);

#endif //OS_FILE_H
