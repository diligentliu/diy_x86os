#include "fs/fs.h"
#include "comm/cpu_instr.h"
#include "comm/boot_info.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "dev/console.h"
#include "fs/file.h"
#include "dev/dev.h"
#include "core/task.h"

static uint8_t TEMP_ADDR[100 * 1024];
static uint8_t *temp_pos;
#define TEMP_FILE_ID 100

void fs_init() {
	file_table_init();
}

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

static int is_path_valid(const char *path) {
	if (path == (const char *) 0 || path[0] == '\0') {
		return 0;
	}
	return 1;
}

int sys_open(const char *path, int flags, ...) {
	if (kernel_strncmp(path, "tty", 3) == 0) {
		if (!is_path_valid(path)) {
			log_printf("sys_open: invalid path\n");
			return -1;
		}
		// 分配一个文件描述符
		int fd = -1;
		file_t *file = file_alloc();
		if (file != (file_t *) 0) {
			fd = task_alloc_fd(file);
			if (fd < 0) {
				goto sys_open_failed;
			}
		} else {
			goto sys_open_failed;
		}

		if (kernel_strlen(path) < 5) {
			goto sys_open_failed;
		}

		int minor = path[4] - '0';
		int dev_id = dev_open(DEV_TTY, minor, 0);
		if (dev_id < 0) {
			goto sys_open_failed;
		}
		file->dev_id = dev_id;
		file->mode = 0;
		file->pos = 0;
		file->ref = 1;
		file->type = FILE_TYPE_TTY;
		kernel_strncpy(file->name, path, FILE_NAME_SIZE);
		return fd;
sys_open_failed:
		if (file != (file_t *) 0) {
			file_free(file);
		}
		if (fd >= 0) {
			task_free_fd(fd);
		}
		return -1;
	} else {
		if (path[0] == '/') {
			read_disk(5000, 80, TEMP_ADDR);
			temp_pos = TEMP_ADDR;
			return TEMP_FILE_ID;
		} else {
			// 读取当前目录
		}
	}
	return -1;
}

int sys_read(int fd, void *buf, int len) {
	if (fd == TEMP_FILE_ID) {
		kernel_memcpy(buf, temp_pos, len);
		temp_pos += len;
		return len;
	} else {
		file_t *file = task_file(fd);
		if (file == (file_t *) 0) {
			log_printf("sys_read: invalid file descriptor\n");
			return -1;
		}
		return dev_read(file->dev_id, 0, buf, len);
	}
}

int sys_write(int fd, char *buf, int len) {
	file_t *file = task_file(fd);
	if (file == (file_t *) 0) {
		log_printf("sys_write: invalid file descriptor\n");
		return -1;
	}
	return dev_write(file->dev_id, 0, buf, len);
}

int sys_lseek(int fd, int offset, int whence) {
	if (fd == TEMP_FILE_ID) {
		temp_pos = (uint8_t *) (TEMP_ADDR + offset);
		return 0;
	}

	return -1;
}

int sys_close(int fd) {
	return 0;
}

int sys_isatty(int fd) {
	return -1;
}

int sys_fstat(int fd, struct stat *st) {
	kernel_memset(st, 0, sizeof(struct stat));
	st->st_size = 0;
	return 0;
}

int sys_dup(int fd) {
	if (fd < 0 || fd >= TASK_OFILE_NR) {
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
		file->ref++;
		return new_fd;
	}

	log_printf("no task file avaliable\n");
	return -1;
}