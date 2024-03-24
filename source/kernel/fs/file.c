#include "fs/file.h"
#include "ipc/mutex.h"
#include "tools/klib.h"

static file_t file_table[FILE_TABLE_SIZE];
static mutex_t file_table_mutex;

file_t *file_alloc() {
	mutex_lock(&file_table_mutex);
	for (int i = 0; i < FILE_TABLE_SIZE; ++i) {
		if (file_table[i].ref == 0) {
			kernel_memset(&file_table[i], 0, sizeof(file_t));
			file_table[i].ref = 1;
			mutex_unlock(&file_table_mutex);
			return file_table + i;
		}
	}
	mutex_unlock(&file_table_mutex);
	return (file_t *) 0;
}

void file_free(file_t *file) {
	mutex_lock(&file_table_mutex);
	if (file->ref) {
		--file->ref;
	}
	mutex_unlock(&file_table_mutex);
}

void file_table_init() {
	mutex_init(&file_table_mutex);
	kernel_memset(file_table, 0, sizeof(file_table));
}

void file_inc_ref(file_t *file) {
	mutex_lock(&file_table_mutex);
	++file->ref;
	mutex_unlock(&file_table_mutex);
}