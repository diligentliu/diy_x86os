#include "dev/disk.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "comm/boot_info.h"
#include "dev/dev.h"
#include "cpu/irq.h"

static mutex_t mutex;
static sem_t op_sem;
static disk_t disk_buf[DISK_CNT];
static int task_on_disk_flag = 0;

static void disk_send_cmd(disk_t *disk, uint32_t start_sector, uint32_t sector_count, uint8_t cmd) {
	outb(DISK_DRIVE(disk), DISK_DRIVE_BASE | disk->drive);
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) sector_count >> 8);
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 24));
	outb(DISK_LBA_MID(disk), 0);
	outb(DISK_LBA_HI(disk), 0);
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) sector_count);
	outb(DISK_LBA_LO(disk), (uint8_t) start_sector);
	outb(DISK_LBA_MID(disk), (uint8_t) (start_sector >> 8));
	outb(DISK_LBA_HI(disk), (uint8_t) (start_sector >> 16));
	outb(DISK_CMD(disk), cmd);
}

static inline void disk_read_data(disk_t *disk, void *buf, int size) {
	uint16_t *data_buf = (uint16_t *) buf;
	for (int i = 0; i < size / 2; ++i) {
		*data_buf++ = inw(DISK_DATA(disk));
	}
}

static inline void disk_write_data(disk_t *disk, void *buf, int size) {
	uint16_t *data_buf = (uint16_t *) buf;
	for (int i = 0; i < size / 2; ++i) {
		outw(DISK_DATA(disk), *data_buf++);
	}
}

static inline int disk_wait(disk_t *disk) {
	uint8_t status;
	do {
		status = inb(DISK_STATUS(disk));
	} while ((status & (DISK_STATUS_BUSY | DISK_STATUS_DRQ | DISK_STATUS_ERR))
	         == DISK_STATUS_BUSY);

	return status & DISK_STATUS_ERR ? -1 : 0;
}

static void print_disk_info(disk_t *disk) {
	log_printf("Disk %s: %s\n", disk->name, disk->drive == DISK_DISK_MASTER ? "master" : "slave");
	log_printf("    port base: %x\n", disk->port_base);
	log_printf("    total size: %dM\n", disk->sector_size * disk->sector_count / 1024 / 1024);

	log_printf("    Partitions:\n");
	for (int i = 0; i < DISK_PRIMARY_PART_CNT; ++i) {
		partinfo_t *part = disk->partinfo + i;
		if (part->type == FS_INVALID) {
			continue;
		}
		log_printf("        %s: %s, %dM\n", part->name, part->type == FS_FAT16_0 ? "FAT16" : "unknown",
		           part->total_sectors * disk->sector_size / 1024 / 1024);
	}
}

static int detect_part_info(disk_t *disk) {
	mbr_t mbr;
	disk_send_cmd(disk, 0, 1, DISK_CMD_READ);
	int err = disk_wait(disk);
	if (err < 0) {
		log_printf("Disk %s read MBR failed\n", disk->name);
		return err;
	}

	disk_read_data(disk, &mbr, sizeof(mbr));
	part_item_t *item = mbr.part_item;
	partinfo_t *part_info = disk->partinfo + 1;
	for (int i = 0; i < MBR_PRIMARY_PART_NR; ++i, ++item, ++part_info) {
		part_info->type = item->system_id;
		if (part_info->type == FS_INVALID) {
			part_info->total_sectors = 0;
			part_info->start_sector = 0;
			part_info->disk = (disk_t *) 0;
		} else {
			kernel_sprintf(part_info->name, "%s%d", disk->name, i + 1);
			part_info->total_sectors = item->total_sectors;
			part_info->start_sector = item->relative_sectors;
			part_info->disk = disk;
		}
	}
}

static int disk_identify(disk_t *disk) {
	disk_send_cmd(disk, 0, 0, DISK_CMD_IDENTIFY);

	int err = inb(DISK_STATUS(disk));
	if (err == 0) {
		log_printf("Disk %s doesn't exist\n", disk->name);
		return -1;
	}

	err = disk_wait(disk);
	if (err < 0) {
		log_printf("Disk %s read failed\n", disk->name);
		return err;
	}

	uint16_t buf[256];
	disk_read_data(disk, buf, sizeof(buf));
	disk->sector_count = *(uint32_t *) (buf + 100);
	disk->sector_size = SECTOR_SIZE;

	partinfo_t *part = &disk->partinfo[0];
	part->disk = disk;
	kernel_sprintf(part->name, "%s%d", disk->name, 0);
	part->start_sector = 0;
	part->total_sectors = disk->sector_count;
	part->type = FS_INVALID;

	detect_part_info(disk);
	return 0;
}

void disk_init() {
	log_printf("Check disk...\n");

	kernel_memset(disk_buf, 0, sizeof(disk_buf));

	mutex_init(&mutex);
	sem_init(&op_sem, 0);
	for (int i = 0; i < DISK_PER_CHANNEL; ++i) {
		disk_t *disk = &disk_buf[i];

		// sda, sdb, sdc, sdd
		kernel_sprintf(disk->name, "sd%c", i + 'a');
		disk->drive = i == 0 ? DISK_DISK_MASTER : DISK_DISK_SLAVE;
		disk->port_base = IOBASE_PRIMARY;
		disk->mutex = &mutex;
		disk->op_sem = &op_sem;

		int err = disk_identify(disk);
		if (err == 0) {
			print_disk_info(disk);
		}
	}
}

int disk_open(device_t *dev) {
	int disk_id = (dev->minor >> 4) - 0xa;
	int part_id = dev->minor & 0xF;

	if (disk_id >= DISK_CNT || part_id >= DISK_PRIMARY_PART_CNT) {
		log_printf("disk_open: invalid disk id or partition id\n");
		return -1;
	}

	disk_t *disk = disk_buf + disk_id;
	if (disk->sector_size == 0) {
		log_printf("disk_open: disk %s doesn't exist\n", disk->name);
		return -1;
	}

	partinfo_t *part_info = disk->partinfo + part_id;
	if (part_info->total_sectors == 0) {
		log_printf("disk_open: partition %s doesn't exist\n", part_info->name);
		return -1;
	}

	dev->data = part_info;
	irq_install(IRQ14_HARDDISK_PRIMARY, (irq_handler_t) exception_handler_ide_primary);
	irq_enable(IRQ14_HARDDISK_PRIMARY);
	return 0;
}

int disk_read(device_t *dev, int addr, char *buf, int size) {
	partinfo_t *part_info = (partinfo_t *) dev->data;
	if (part_info == (partinfo_t *) 0) {
		log_printf("disk_read: invalid partition\n");
		return -1;
	}

	disk_t *disk = part_info->disk;
	if (disk == (disk_t *) 0) {
		log_printf("disk_read: invalid disk\n");
		return -1;
	}

	mutex_lock(disk->mutex);
	task_on_disk_flag = 1;

	int sector_cnt;
	disk_send_cmd(disk, part_info->start_sector + addr, size, DISK_CMD_READ);
	for (sector_cnt = 0; sector_cnt < size; ++sector_cnt, buf += disk->sector_size) {
		if (task_current() != (task_t *) 0) {
			sem_p(disk->op_sem);
		}
		
		int err = disk_wait(disk);
		if (err < 0) {
			log_printf("disk_read: disk(%s) read error: start sect %d, count %d", disk->name, addr, sector_cnt);
			break;
		}
		disk_read_data(disk, buf, disk->sector_size);
	}

	mutex_unlock(disk->mutex);
	return sector_cnt;
}

int disk_write(device_t *dev, int addr, const char *buf, int size) {
	partinfo_t *part_info = (partinfo_t *) dev->data;
	if (part_info == (partinfo_t *) 0) {
		log_printf("disk_write: invalid partition\n");
		return -1;
	}

	disk_t *disk = part_info->disk;
	if (disk == (disk_t *) 0) {
		log_printf("disk_write: invalid disk\n");
		return -1;
	}

	mutex_lock(disk->mutex);
	task_on_disk_flag = 1;

	int sector_cnt;
	disk_send_cmd(disk, part_info->start_sector + addr, size, DISK_CMD_WRITE);
	for (sector_cnt = 0; sector_cnt < size; ++sector_cnt, buf += disk->sector_size) {
		disk_write_data(disk, (void *) buf, disk->sector_size);
		if (task_current() != (task_t *) 0) {
			sem_p(disk->op_sem);
		}

		int err = disk_wait(disk);
		if (err < 0) {
			log_printf("disk_write: disk(%s) read error: start sect %d, count %d", disk->name, addr, sector_cnt);
			break;
		}
	}

	mutex_unlock(disk->mutex);
	return sector_cnt;
}

int disk_control(device_t *dev, int cmd, int arg0, int arg1) {
	return 0;
}

void disk_close(device_t *dev) {

}

void do_handler_ide_primary(exception_frame_t *frame) {
	// log_printf("do_handler_ide_primary\n");
	pic_send_eoi(IRQ14_HARDDISK_PRIMARY);
	if (task_on_disk_flag && task_current() != (task_t *) 0) {
		sem_v(&op_sem);
	}
}

dev_desc_t dev_disk_desc = {
		.name = "disk",
		.major = DEV_TYPE_DISK,
		.open = disk_open,
		.read = disk_read,
		.write = disk_write,
		.control = disk_control,
		.close = disk_close,
};