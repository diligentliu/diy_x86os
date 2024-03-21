#include "fs/fs.h"
#include "comm/cpu_instr.h"
#include "comm/boot_info.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "dev/console.h"

static uint8_t TEMP_ADDR[100 * 1024];
static uint8_t *temp_pos;
#define TEMP_FILE_ID 100

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

int sys_open(const char *path, int flags, ...) {
	if (path[0] == '/') {
		read_disk(5000, 80, TEMP_ADDR);
		temp_pos = TEMP_ADDR;
		return TEMP_FILE_ID;
	} else {
		// 读取当前目录
	}
	return -1;
}

int sys_read(int fd, void *buf, int len) {
	if (fd == TEMP_FILE_ID) {
		kernel_memcpy(buf, temp_pos, len);
		temp_pos += len;
		return len;
	}
	return -1;
}

int sys_write(int fd, char *buf, int len) {
	// buf[len] = '\0';
	console_write(0, buf, len);
	// log_printf("%s", buf);
	return len;
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
	return -1;
}
