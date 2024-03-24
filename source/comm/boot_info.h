/**
 * 系统启动信息
 */
#ifndef OS_BOOT_INFO_H
#define OS_BOOT_INFO_H

#include "types.h"

#define BOOT_RAM_REGION_MAX            10        // RAM区最大数量

/**
 * 启动信息参数
 */
typedef struct _boot_info_t {
	// RAM区信息
	struct {
		uint32_t start;
		uint32_t size;
	} ram_region_cfg[BOOT_RAM_REGION_MAX];
	int ram_region_count;
} boot_info_t;

#define SECTOR_SIZE        512            // 磁盘扇区大小
#define SYS_KERNEL_LOAD_ADDR        (1024*1024)        // 内核加载的起始地址

#endif // OS_BOOT_INFO_H
