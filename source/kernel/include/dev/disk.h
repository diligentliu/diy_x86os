#ifndef OS_DISK_H
#define OS_DISK_H

#include "comm/types.h"
#include "ipc/mutex.h"
#include "ipc/sem.h"

#define PART_NAME_SIZE              32      // 分区名称
#define DISK_NAME_SIZE              32      // 磁盘名称大小
#define DISK_CNT                    2       // 磁盘的数量
#define DISK_PRIMARY_PART_CNT       (4 + 1) // 主分区数量最多 4 个
#define DISK_PER_CHANNEL            2       // 每个通道最多 2 个磁盘

// https://wiki.osdev.org/ATA_PIO_Mode#IDENTIFY_command
// 只考虑支持主总结primary bus
#define IOBASE_PRIMARY              0x1F0
#define	DISK_DATA(disk)				(disk->port_base + 0)		// 数据寄存器
#define	DISK_ERROR(disk)			(disk->port_base + 1)		// 错误寄存器
#define	DISK_SECTOR_COUNT(disk)		(disk->port_base + 2)		// 扇区数量寄存器
#define	DISK_LBA_LO(disk)			(disk->port_base + 3)		// LBA寄存器
#define	DISK_LBA_MID(disk)			(disk->port_base + 4)		// LBA寄存器
#define	DISK_LBA_HI(disk)			(disk->port_base + 5)		// LBA寄存器
#define	DISK_DRIVE(disk)			(disk->port_base + 6)		// 磁盘或磁头？
#define	DISK_STATUS(disk)			(disk->port_base + 7)		// 状态寄存器
#define	DISK_CMD(disk)				(disk->port_base + 7)		// 命令寄存器

// ATA命令
#define	DISK_CMD_IDENTIFY	        0xEC	    // IDENTIFY命令
#define	DISK_CMD_READ				0x24	    // 读命令
#define	DISK_CMD_WRITE				0x34	    // 写命令

// 状态寄存器
#define DISK_STATUS_ERR             (1 << 0)    // 发生了错误
#define DISK_STATUS_DRQ             (1 << 3)    // 准备好接受数据或者输出数据
#define DISK_STATUS_DF              (1 << 5)    // 驱动错误
#define DISK_STATUS_BUSY            (1 << 7)    // 正忙

#define	DISK_DRIVE_BASE		        0xE0		// 驱动器号基础值:0xA0 + LBA

#pragma pack(1)

typedef struct _part_item_t {
	uint8_t boot_active;            // 分区是否活动
	uint8_t start_header;           // 起始 header
	uint16_t start_sector : 6;      // 起始扇区
	uint16_t start_cylinder : 10;   // 起始磁道
	uint8_t system_id;              // 分区类型
	uint8_t end_header;             // 结束 header
	uint16_t end_sector : 6;        // 结束扇区
	uint16_t end_cylinder : 10;     // 结束磁道
	uint32_t relative_sectors;      // 相对扇区
	uint32_t total_sectors;         // 总扇区
} part_item_t;

#define MBR_PRIMARY_PART_NR         4           // 分区表数量

typedef struct _mbr_t {
	uint8_t code[446];                          // 引导代码
	part_item_t part_item[MBR_PRIMARY_PART_NR]; // 分区表
	uint8_t boot_sign[2];                       // 引导标志
} mbr_t;

#pragma pack()

struct _disk_t;

typedef struct _partinfo_t {
	char name[PART_NAME_SIZE];
	struct _disk_t *disk;

	enum {
		FS_INVALID = 0x00,
		FS_FAT16_0 = 0x06,
		FS_FAT16_1 = 0x0E,
	} type;

	uint32_t start_sector;
	uint32_t total_sectors;
} partinfo_t;

typedef struct _disk_t {
	char name[DISK_NAME_SIZE];

	enum {
		DISK_DISK_MASTER = (0 << 4),
		DISK_DISK_SLAVE = (1 << 4),
	} drive;

	uint16_t port_base;
	uint32_t sector_size;
	uint32_t sector_count;
	partinfo_t partinfo[DISK_PRIMARY_PART_CNT];

	mutex_t *mutex;
	sem_t *op_sem;
} disk_t;

void disk_init();

void exception_handler_ide_primary();

#endif //OS_DISK_H
