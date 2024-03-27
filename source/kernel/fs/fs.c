#include "fs/fs.h"
#include "comm/cpu_instr.h"
#include "comm/boot_info.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "dev/console.h"
#include "fs/file.h"
#include "dev/dev.h"
#include "core/task.h"
#include "fs/devfs/devfs.h"
#include "dev/disk.h"
#include "os_cfg.h"
#include <sys/file.h>

#define FS_TABLE_SIZE 16
static list_t mounted_list;
static list_t free_list;
static fs_t fs_table[FS_TABLE_SIZE];

extern fs_op_t devfs_op;
extern fs_op_t fatfs_op;
static fs_t *root_fs;

static uint8_t TEMP_ADDR[100 * 1024];
static uint8_t *temp_pos;
#define TEMP_FILE_ID 100

static fs_op_t *get_fs_op(fs_type_t type, int major) {
	switch (type) {
		case FS_TYPE_DEV:
			return &devfs_op;
		case FS_TYPE_FAT16:
			return &fatfs_op;
		default:
			return (fs_op_t *) 0;
	}
}

static fs_t *mount(fs_type_t type, const char *mount_point, int major, int minor) {
	fs_t *fs = (fs_t *) 0;
	log_printf("mount file system: %s, dev: %x\n", mount_point, major);

	list_node_t *node = list_first(&mounted_list);
	while (node) {
		fs_t *fs_tmp = list_node_parent(node, fs_t, node);
		if (kernel_strncmp(fs_tmp->mount_point, mount_point, FS_MOUNT_POINT_LEN) == 0) {
			log_printf("mount point already exists\n");
			goto mount_failed;
		}
		node = list_node_next(node);
	}

	list_node_t *free_node = list_pop_front(&free_list);
	if (free_node == (list_node_t *) 0) {
		log_printf("no free fs node\n");
		goto mount_failed;
	}

	fs = list_node_parent(free_node, fs_t, node);

	fs_op_t *op = get_fs_op(type, major);
	if (op == (fs_op_t *) 0) {
		log_printf("unsupported fs type: %d\n", type);
		goto mount_failed;
	}

	kernel_memset(fs, 0, sizeof(fs_t));
	kernel_strncpy(fs->mount_point, mount_point, FS_MOUNT_POINT_LEN);
	fs->op = op;

	if (fs->op->mount(fs, major, minor) < 0) {
		log_printf("mount failed\n");
		goto mount_failed;
	}

	list_push_back(&mounted_list, &fs->node);
	return fs;
mount_failed:
	if (fs != (fs_t *) 0) {
		list_push_back(&free_list, &fs->node);
	}
	return (fs_t *) 0;
}

static void mount_list_init() {
	list_init(&mounted_list);
	list_init(&free_list);
	for (int i = 0; i < FS_TABLE_SIZE; i++) {
		list_push_front(&free_list, &fs_table[i].node);
	}
}

void fs_init() {
	mount_list_init();
	file_table_init();

	disk_init();

	fs_t *fs = mount(FS_TYPE_DEV, "/dev", 0, 0);
	ASSERT(fs != (fs_t *) 0);

	root_fs = mount(FS_TYPE_FAT16, "/home", ROOT_DEV);
	ASSERT(root_fs != (fs_t *) 0);
}

#if 0
static void read_disk(uint32_t sector, uint32_t sector_count, uint8_t *buf) {
	outb(0x1F6, (uint8_t) (0xE0));

	outb(0x1F2, (uint8_t) (sector_count >> 8));
	outb(0x1F3, (uint8_t) (sector >> 24));        // LBA参数的24~31位
	outb(0x1F4, (uint8_t) (0));                    // LBA参数的32~39位
	outb(0x1F5, (uint8_t) (0));                    // LBA参数的40~47位

	outb(0x1F2, (uint8_t) (sector_count));
	outb(0x1F3, (uint8_t) (sector));            // LBA参数的0~7位
	outb(0x1F4, (uint8_t) (sector >> 8));        // LBA参数的8~15位
	outb(0x1F5, (uint8_t) (sector >> 16));        // LBA参数的16~23位

	outb(0x1F7, (uint8_t) 0x24);

	// 读取数据
	uint16_t *data_buf = (uint16_t *) buf;
	while (sector_count-- > 0) {
		// 每次扇区读之前都要检查，等待数据就绪
		while ((inb(0x1F7) & 0x88) != 0x8) {}

		// 读取并将数据写入到缓存中
		for (int i = 0; i < SECTOR_SIZE / 2; i++) {
			*data_buf++ = inw(0x1F0);
		}
	}
}
#endif

static int is_fd_bad(int fd) {
	return fd < 0 || fd >= TASK_OFILE_NR;
}

static int is_path_valid(const char *path) {
	if (path == (const char *) 0 || path[0] == '\0') {
		return 0;
	}
	return 1;
}

int path_to_num(const char *path, int *num) {
	*num = 0;
	const char *c = path;
	while (*c) {
		if (*c < '0' || *c > '9') {
			return -1;
		}
		*num = *num * 10 + (*c - '0');
		c++;
	}
	return 0;
}

int path_begin_with(const char *path, const char *str) {
	const char *s1 = path, *s2 = str;
	while (*s1 && *s2 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *s2 == '\0';
}

const char *path_next_child(const char *path) {
	// /dev/tty0
	const char *c = path;
	while (*c && *c++ == '/');
	while (*c && *c++ != '/');
	return *c ? c : (const char *) 0;
}

static void fs_protect(fs_t *fs) {
	if (fs->mutex) {
		mutex_lock(fs->mutex);
	}
}

static void fs_unprotect(fs_t *fs) {
	if (fs->mutex) {
		mutex_unlock(fs->mutex);
	}
}

int sys_open(const char *path, int flags, ...) {
	if (kernel_strncmp(path, "/shell.elf", 10) == 0) {
		// read_disk(5000, 80, TEMP_ADDR);
		// temp_pos = TEMP_ADDR;
		int dev_id = dev_open(DEV_TYPE_DISK, 0xA0, (void *) 0);
		dev_read(dev_id, 5000, TEMP_ADDR, 80);
		temp_pos = (uint8_t *) TEMP_ADDR;
		return TEMP_FILE_ID;
	}

	file_t *file = file_alloc();
	if (!file) {
		return -1;
	}

	int fd = task_alloc_fd(file);
	if (fd < 0) {
		goto sys_open_failed;
	}

	// 检查名称是否以挂载点开头，如果没有，则认为name在根目录下
	// 即只允许根目录下的遍历
	fs_t *fs = (fs_t *) 0;
	list_node_t *node = list_first(&mounted_list);
	while (node) {
		fs_t *curr = list_node_parent(node, fs_t, node);
		if (path_begin_with(path, curr->mount_point)) {
			fs = curr;
			break;
		}
		node = list_node_next(node);
	}

	if (fs) {
		path = path_next_child(path);
	} else {
		// 如果没有挂载点，则认为是根目录
		fs = root_fs;
	}

	file->mode = flags;
	file->fs = fs;
	kernel_strncpy(file->name, path, FILE_NAME_SIZE);

	fs_protect(fs);
	int err = fs->op->open(fs, path, file);
	if (err < 0) {
		fs_unprotect(fs);

		log_printf("open %s failed.", path);
		return -1;
	}
	fs_unprotect(fs);

	return fd;

sys_open_failed:
	file_free(file);
	if (fd >= 0) {
		task_free_fd(fd);
	}
	return -1;
}

int sys_read(int fd, void *buf, int len) {
	if (fd == TEMP_FILE_ID) {
		kernel_memcpy(buf, temp_pos, len);
		temp_pos += len;
		return len;
	}

	if (is_fd_bad(fd)) {
		log_printf("sys_read: invalid file descriptor\n");
		return 0;
	}

	file_t *file = task_file(fd);
	if (file == (file_t *) 0) {
		log_printf("sys_read: file not opened\n");
		return 0;
	}

	if (file->mode == O_WRONLY) {
		log_printf("sys_read: file is write only\n");
		return 0;
	}

	fs_t *fs = file->fs;
	fs_protect(fs);
	int size = fs->op->read(buf, len, file);
	fs_unprotect(fs);
	return size;
}

int sys_write(int fd, char *buf, int len) {
	if (is_fd_bad(fd) || !buf || !len) {
		return 0;
	}

	file_t *file = task_file(fd);
	if (file == (file_t *) 0) {
		log_printf("sys_write: file not opened\n");
		return 0;
	}

	if (file->mode == O_RDONLY) {
		log_printf("sys_write: file is read only\n");
		return 0;
	}

	fs_t *fs = file->fs;
	fs_protect(fs);
	int size = fs->op->write(buf, len, file);
	fs_unprotect(fs);
	return size;
}

int sys_lseek(int fd, int offset, int whence) {
	if (fd == TEMP_FILE_ID) {
		temp_pos = (uint8_t *) (offset + TEMP_ADDR);
		return 0;
	}

	if (is_fd_bad(fd)) {
		return -1;
	}

	file_t *file = task_file(fd);
	if (!file) {
		log_printf("file not opened\n");
		return -1;
	}

	fs_t *fs = file->fs;

	fs_protect(fs);
	int err = fs->op->seek(file, offset, whence);
	fs_unprotect(fs);
	return err;
}

int sys_close(int fd) {
	if (fd == TEMP_FILE_ID) {
		return 0;
	}

	if (is_fd_bad(fd)) {
		log_printf("sys_close: invalid file descriptor\n");
		return -1;
	}

	file_t *file = task_file(fd);
	if (file == (file_t *) 0) {
		log_printf("sys_close: file not opened\n");
		return -1;
	}

	ASSERT(file->ref > 0);

	if (file->ref-- == 1) {
		fs_t *fs = file->fs;
		fs_protect(fs);
		fs->op->close(file);
		fs_unprotect(fs);
		file_free(file);
	}

	task_free_fd(fd);
	return 0;
}

int sys_isatty(int fd) {
	if (is_fd_bad(fd)) {
		log_printf("sys_isatty: invalid file descriptor\n");
		return 0;
	}

	file_t *file = task_file(fd);
	if (file == (file_t *) 0) {
		log_printf("sys_isatty: file not opened\n");
		return 0;
	}

	return file->type == FILE_TYPE_TTY;
}

int sys_fstat(int fd, struct stat *st) {
	if (is_fd_bad(fd)) {
		return -1;
	}

	file_t *file = task_file(fd);
	if (file == (file_t *) 0) {
		log_printf("sys_fstat: file not opened\n");
		return -1;
	}

	fs_t *fs = file->fs;

	kernel_memset(st, 0, sizeof(struct stat));

	fs_protect(fs);
	int err = fs->op->stat(file, st);
	fs_unprotect(fs);
	return err;
}

int sys_dup(int fd) {
	if (is_fd_bad(fd)) {
		log_printf("sys_dup: invalid file descriptor\n");
		return -1;
	}
	file_t *file = task_file(fd);
	if (file == (file_t *) 0) {
		log_printf("sys_dup: invalid file descriptor\n");
		return -1;
	}

	int new_fd = task_alloc_fd(file);
	if (new_fd >= 0) {
		file_inc_ref(file);
		return new_fd;
	}

	log_printf("no task file avaliable\n");
	return -1;
}

int sys_opendir(const char *path, DIR *dir) {
	fs_protect(root_fs);
	int err = root_fs->op->opendir(root_fs, path, dir);
	fs_unprotect(root_fs);
	return err;
}

int sys_readdir(DIR *dir, struct dirent *dirent) {
	fs_protect(root_fs);
	int err = root_fs->op->readdir(root_fs, dir, dirent);
	fs_unprotect(root_fs);
	return err;
}

int sys_closedir(DIR *dir) {
	fs_protect(root_fs);
	int err = root_fs->op->closedir(root_fs, dir);
	fs_unprotect(root_fs);
	return err;
}