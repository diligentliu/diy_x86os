#include "fs/fatfs/fatfs.h"
#include "fs/fs.h"
#include "dev/dev.h"
#include "tools/log.h"
#include "core/memory.h"
#include "tools/klib.h"

int fatfs_mount(struct _fs_t *fs, int major, int minor) {
	int dev_id = dev_open(major, minor, (void *) 0);
	if (dev_id < 0) {
		log_printf("fatfs_mount: open disk failed. major: %x, minor: %x\n", major, minor);
		return -1;
	}

	dbr_t *dbr = (dbr_t *) memory_alloc_page();
	if (dbr == (dbr_t *) 0) {
		log_printf("fatfs_mount: alloc memory failed\n");
		goto mount_failed;
	}

	int cnt = dev_read(dev_id, 0, (char *) dbr, 1);
	if (cnt < 1) {
		log_printf("fatfs_mount: read dbr failed\n");
		goto mount_failed;
	}

	fat_t *fat = &fs->fat_data;
	fat->bytes_per_sec = dbr->BPB_BytsPerSec;
	fat->tbl_start = dbr->BPB_RsvdSecCnt;
	fat->tbl_sectors = dbr->BPB_FATSz16;
	fat->tbl_cnt = dbr->BPB_NumFATs;
	fat->root_ent_cnt = dbr->BPB_RootEntCnt;
	fat->sec_per_cluster = dbr->BPB_SecPerClus;
	fat->root_start = fat->tbl_start + fat->tbl_cnt * fat->tbl_sectors;
	fat->data_start = fat->root_start + fat->root_ent_cnt * 32 / SECTOR_SIZE;
	fat->cluster_byte_size = fat->sec_per_cluster * fat->bytes_per_sec;
	fat->fat_buffer = (uint8_t *) dbr;
	fat->fs = fs;

	if (fat->tbl_cnt != 2) {
		log_printf("fatfs_mount: fat table count is not 2\n");
		goto mount_failed;
	}

	if (kernel_memcmp(dbr->BS_FileSysType, "FAT16", 5) != 0) {
		log_printf("fatfs_mount: file system type is not FAT16\n");
		goto mount_failed;
	}

	fs->type = FS_TYPE_FAT16;
	fs->data = &fs->fat_data;
	fs->dev_id = dev_id;
	return 0;
mount_failed:
	if (dbr != (dbr_t *) 0) {
		memory_free_page((uint32_t) dbr);
	}
	dev_close(dev_id);
	return -1;
}

void fatfs_unmount(struct _fs_t *fs) {
	fat_t *fat = (fat_t *) fs->data;
	dev_close(fs->dev_id);
	memory_free_page((uint32_t) fat->fat_buffer);
}

int fatfs_open(struct _fs_t *fs, const char *path, file_t *file) {
	return -1;
}

int fatfs_read(void *buf, int len, file_t *file) {
	return 0;
}

int fatfs_write(char *buf, int len, file_t *file) {
	return 0;
}

void fatfs_close(file_t *file) {

}

int fatfs_seek(file_t *file, int offset, int whence) {
	return -1;
}

int fatfs_stat(file_t *file, struct stat *st) {
	return -1;
}

int fatfs_opendir(struct _fs_t *fs, const char *path, DIR *dir) {
	dir->index = 0;
	return 0;
}

static int bread_sector(fat_t *fat, int sector) {
	if (sector == fat->current_sector) {
		return 0;
	}

	int cnt = dev_read(fat->fs->dev_id, sector, (char *) fat->fat_buffer, 1);
	if (cnt == 1) {
		fat->current_sector = sector;
		return 0;
	}
	return -1;
}

static diritem_t *read_dir_entry(fat_t *fat, int index) {
	if (index < 0 || index >= fat->root_ent_cnt) {
		return (diritem_t *) 0;
	}

	int offset = index * sizeof(diritem_t);
	int err = bread_sector(fat, fat->root_start + offset / fat->bytes_per_sec);
	if (err < 0) {
		return (diritem_t *) 0;
	}
	return (diritem_t *) (fat->fat_buffer + offset % fat->bytes_per_sec);
}

static file_type_t diritem_get_type(diritem_t *item) {
	file_type_t type = FILE_TYPE_UNKNOWN;

	if (item->DIR_Attr & (DIRITEM_ATTR_VOLUME_ID | DIRITEM_ATTR_HIDDEN | DIRITEM_ATTR_SYSTEM)) {
		return FILE_TYPE_UNKNOWN;
	}

	if ((item->DIR_Attr & DIRITEM_ATTR_LONG_NAME) == DIRITEM_ATTR_LONG_NAME) {
		return FILE_TYPE_UNKNOWN;
	}

	return item->DIR_Attr & DIRITEM_ATTR_DIRECTORY ? FILE_TYPE_DIR : FILE_TYPE_NORMAL;
}

static void diritem_get_name(diritem_t *item, char *name) {
	char *c = name;
	char *ext = (char *) 0;
	kernel_memset(name, 0, SFN_LEN + 1);
	for (int i = 0; i < SFN_LEN; ++i) {
		if (item->DIR_Name[i] != ' ') {
			*c++ = item->DIR_Name[i];
		}

		if (i == 7) {
			ext = c;
			*c++ = '.';
		}
	}

	if (ext != (char *) 0 && ext[1] == '\0') {
		ext[0] = '\0';
	}
}

int fatfs_readdir(struct _fs_t *fs, DIR *dir, struct dirent *dirent) {
	fat_t *fat = (fat_t *) fs->data;

	while (dir->index < fat->root_ent_cnt) {
		diritem_t *item = read_dir_entry(fat, dir->index);
		if (item == (diritem_t *) 0) {
			return -1;
		}

		if (item->DIR_Name[0] == DIRITEM_NAME_END) {
			break;
		}

		if (item->DIR_Name[0] != DIRITEM_NAME_FREE) {
			file_type_t type = diritem_get_type(item);
			if (type == FILE_TYPE_NORMAL || type == FILE_TYPE_DIR) {
				dirent->index = dir->index++;
				dirent->type = diritem_get_type(item);
				dirent->size = item->DIR_FileSize;
				diritem_get_name(item, dirent->name);
				return 0;
			}
		}

		dir->index++;
	}
	return -1;
}

int fatfs_closedir(struct _fs_t *fs, DIR *dir) {
	return 0;
}

fs_op_t fatfs_op = {
		.mount = fatfs_mount,
		.unmount = fatfs_unmount,
		.open = fatfs_open,
		.read = fatfs_read,
		.write = fatfs_write,
		.close = fatfs_close,
		.seek = fatfs_seek,
		.stat = fatfs_stat,

		.opendir = fatfs_opendir,
		.readdir = fatfs_readdir,
		.closedir = fatfs_closedir,
};