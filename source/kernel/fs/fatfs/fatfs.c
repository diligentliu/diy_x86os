#include "fs/fatfs/fatfs.h"
#include "fs/fs.h"
#include "dev/dev.h"
#include "tools/log.h"
#include "core/memory.h"
#include "tools/klib.h"
#include "sys/fcntl.h"

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

static int bwrite_sector(fat_t *fat, int sector) {
	int cnt = dev_write(fat->fs->dev_id, sector, (char *) fat->fat_buffer, 1);
	if (cnt == 1) {
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

static void to_sfn(char *sfn, const char *name) {
	char *dest = sfn;
	const char *src = name;
	kernel_memset(dest, ' ', SFN_LEN);
	char *cur = dest;
	char *end = dest + SFN_LEN;
	while (*src && cur < end) {
		char c = *src++;
		switch (c) {
			case '.':
				cur = dest + 8;
				break;
			default:
				if (c >= 'a' && c <= 'z') {
					c -= 'a' - 'A';
				}
				*cur++ = c;
				break;
		}
	}
}

static int diritem_name_match(diritem_t *item, const char *name) {
	char sfn[SFN_LEN];
	to_sfn(sfn, name);
	return kernel_memcmp(sfn, item->DIR_Name, SFN_LEN) == 0;
}

static void read_from_diritem(fat_t *fat, file_t *file, diritem_t *item, int index) {
	file->type = diritem_get_type(item);
	file->size = item->DIR_FileSize;
	file->pos = 0;
	file->index = index;
	file->sblk = (item->DIR_FstClusHI << 16) | item->DIR_FstClusLO;
	file->cblk = file->sblk;
}

static int cluster_is_valid(cluster_t cluster) {
	return cluster < FAT_CLUSTER_INVALID && cluster >= 0x2;     // 值是否正确
}

static int cluster_get_next(fat_t *fat, cluster_t cur_cluster) {
	if (!cluster_is_valid(cur_cluster)) {
		return FAT_CLUSTER_INVALID;
	}

	int offset = cur_cluster * sizeof(cluster_t);
	int sector = offset / fat->bytes_per_sec;
	int offset_sector = offset % fat->bytes_per_sec;

	if (sector >= fat->tbl_sectors) {
		log_printf("cluster_get_next: sector out of range\n");
		return FAT_CLUSTER_INVALID;
	}

	int err = bread_sector(fat, fat->tbl_start + sector);
	if (err < 0) {
		log_printf("cluster_get_next: read sector failed\n");
		return FAT_CLUSTER_INVALID;
	}

	return *((cluster_t *) (fat->fat_buffer + offset_sector));
}

static int cluster_set_next(fat_t *fat, cluster_t cur_cluster, cluster_t next_cluster) {
	if (!cluster_is_valid(cur_cluster)) {
		return -1;
	}

	int offset = cur_cluster * sizeof(cluster_t);
	int sector = offset / fat->bytes_per_sec;
	int offset_sector = offset % fat->bytes_per_sec;

	if (sector >= fat->tbl_sectors) {
		log_printf("cluster_set_next: sector out of range\n");
		return -1;
	}

	int err = bread_sector(fat, fat->tbl_start + sector);
	if (err < 0) {
		log_printf("cluster_set_next: read sector failed\n");
		return -1;
	}

	*((cluster_t *) (fat->fat_buffer + offset_sector)) = next_cluster;

	for (int i = 0; i < fat->tbl_cnt; ++i) {
		err = bwrite_sector(fat, fat->tbl_start + sector + i * fat->tbl_sectors);
		if (err < 0) {
			log_printf("cluster_set_next: write sector failed\n");
			return -1;
		}
	}
	return 0;
}

static void cluster_free_chain(fat_t *fat, cluster_t cluster) {
	while (cluster_is_valid(cluster)) {
		cluster_t next = cluster_get_next(fat, cluster);
		cluster_set_next(fat, cluster, FAT_CLUSTER_FREE);
		cluster = next;
	}
}

static cluster_t cluster_alloc_free(fat_t *fat, int cnt) {
	cluster_t pre = FAT_CLUSTER_INVALID, start = FAT_CLUSTER_INVALID;
	int cluster_cnt = fat->tbl_sectors * fat->bytes_per_sec / sizeof(cluster_t);
	for (int cur = 2; cur < cluster_cnt && cnt; ++cur) {
		cluster_t free = cluster_get_next(fat, cur);
		if (free == FAT_CLUSTER_FREE) {
			if (!cluster_is_valid(start)) {
				start = cur;
			}

			if (cluster_is_valid(pre)) {
				int err = cluster_set_next(fat, pre, cur);
				if (err < 0) {
					log_printf("cluster_alloc_free: set next cluster failed\n");
					return FAT_CLUSTER_INVALID;
				}
			}

			pre = cur;
			cnt--;
		}
	}

	if (cnt == 0) {
		int err = cluster_set_next(fat, pre, FAT_CLUSTER_INVALID);
		if (err == 0) {
			return start;
		}
	}

	cluster_free_chain(fat, start);
	return FAT_CLUSTER_INVALID;
}

static int expand_file(file_t *file, int inc_size) {
	fat_t *fat = (fat_t *) file->fs->data;

	int cluster_cnt;
	if (file->size == 0 || file->size % fat->cluster_byte_size == 0) {
		cluster_cnt = up2(inc_size, fat->cluster_byte_size) / fat->cluster_byte_size;
	} else {
		int cur_cluster_free = fat->cluster_byte_size - file->size % fat->cluster_byte_size;
		if (cur_cluster_free > inc_size) {
			return 0;
		}

		cluster_cnt = up2(inc_size - cur_cluster_free, fat->cluster_byte_size) / fat->cluster_byte_size;
		if (cluster_cnt == 0) {
			cluster_cnt = 1;
		}
	}

	cluster_t start = cluster_alloc_free(fat, cluster_cnt);
	if (!cluster_is_valid(start)) {
		log_printf("expand_file: alloc cluster failed\n");
		return -1;
	}

	if (!cluster_is_valid(file->sblk)) {
		file->sblk = start;
		file->cblk = start;
	} else {
		int err = cluster_set_next(fat, file->cblk, start);
		if (err < 0) {
			log_printf("expand_file: set next cluster failed\n");
			return -1;
		}
	}
	return 0;
}

static int move_file_pos(file_t *file, fat_t *fat, int offset, int expand) {
	uint32_t cur_offset = file->pos % fat->cluster_byte_size;
	if (cur_offset + offset >= fat->cluster_byte_size) {
		cluster_t next = cluster_get_next(fat, file->cblk);
		if (next == FAT_CLUSTER_INVALID && expand) {
			int err = expand_file(file, fat->cluster_byte_size);
			if (err < 0) {
				return -1;
			}

			next = cluster_get_next(fat, file->cblk);
		}
		file->cblk = next;
	}

	file->pos += offset;
	return 0;
}

static int diritem_init(diritem_t *item, uint8_t attr, const char *name) {
	to_sfn((char *) item->DIR_Name, name);
	item->DIR_FstClusHI = (uint16_t) (FAT_CLUSTER_INVALID >> 16);
	item->DIR_FstClusLO = (uint16_t) (FAT_CLUSTER_INVALID & 0xFFFF);
	item->DIR_FileSize = 0;
	item->DIR_Attr = attr;
	item->DIR_NTRes = 0;

	// 时间写固定值，简单方便
	item->DIR_CrtTime = 0;
	item->DIR_CrtDate = 0;
	item->DIR_WrtTime = item->DIR_CrtTime;
	item->DIR_WrtDate = item->DIR_CrtDate;
	item->DIR_LstAccDate = item->DIR_CrtDate;
	return 0;
}

static int write_dir_entry(fat_t *fat, diritem_t *item, int index) {
	if (index < 0 || index >= fat->root_ent_cnt) {
		return -1;
	}

	int offset = index * sizeof(diritem_t);
	int sector = fat->root_start + offset / fat->bytes_per_sec;
	int err = bread_sector(fat, sector);
	if (err < 0) {
		return -1;
	}
	kernel_memcpy(fat->fat_buffer + offset % fat->bytes_per_sec, item, sizeof(diritem_t));
	return bwrite_sector(fat, sector);
}

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
	mutex_init(&fat->mutex);
	fs->mutex = &fat->mutex;

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
	fat_t *fat = (fat_t *) fs->data;
	diritem_t *file_item = (diritem_t *) 0;
	int index = -1;
	for (int i = 0; i < fat->root_ent_cnt; ++i) {
		diritem_t *item = read_dir_entry(fat, i);
		if (item == (diritem_t *) 0) {
			return -1;
		}

		if (item->DIR_Name[0] == DIRITEM_NAME_END) {
			if (index == -1) {
				index = i;
			}
			break;
		}

		if (item->DIR_Name[0] == DIRITEM_NAME_FREE) {
			if (index == -1) {
				index = i;
			}
			continue;
		}

		if (diritem_name_match(item, path)) {
			file_item = item;
			index = i;
			break;
		}
	}

	if (file_item != (diritem_t *) 0) {
		read_from_diritem(fat, file, file_item, index);

		if (file->mode & O_TRUNC) {
			cluster_free_chain(fat, file->sblk);
			file->size = 0;
			file->pos = 0;
			file->cblk = file->sblk = FAT_CLUSTER_INVALID;
		}
		return 0;
	} else if ((file->mode & O_CREAT) && index >= 0) {
		diritem_t item;
		diritem_init(&item, 0, path);
		int err = write_dir_entry(fat, &item, index);
		if (err < 0) {
			log_printf("create file failed.");
			return -1;
		}

		read_from_diritem(fat, file, &item, index);
		return 0;
	}
	return -1;
}

int fatfs_read(void *buf, int len, file_t *file) {
	fat_t *fat = (fat_t *) file->fs->data;

	uint32_t nbytes = len;
	if (file->pos + nbytes > file->size) {
		nbytes = file->size - file->pos;
	}

	uint32_t total_read = 0;
	while (nbytes > 0) {
		uint32_t cur_read = nbytes;
		uint32_t cluster_offset = file->pos % fat->cluster_byte_size;
		uint32_t start_sector = fat->data_start + (file->cblk - 2) * fat->sec_per_cluster;

		if (cluster_offset == 0 && nbytes == fat->cluster_byte_size) {
			int err = dev_read(fat->fs->dev_id, start_sector, (char *) buf, fat->sec_per_cluster);
			if (err < 0) {
				return total_read;
			}
			cur_read = fat->cluster_byte_size;
		} else {
			if (cluster_offset + cur_read > fat->cluster_byte_size) {
				cur_read = fat->cluster_byte_size - cluster_offset;
			}
			fat->current_sector = -1;
			int err = dev_read(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
			if (err < 0) {
				return total_read;
			}
			kernel_memcpy(buf, fat->fat_buffer + cluster_offset, cur_read);
		}

		buf += cur_read;
		nbytes -= cur_read;
		total_read += cur_read;

		int err = move_file_pos(file, fat, cur_read, 0);
		if (err < 0) {
			return total_read;
		}
	}
	return total_read;
}

int fatfs_write(char *buf, int len, file_t *file) {
	fat_t *fat = (fat_t *) file->fs->data;

	if (file->pos + len > file->size) {
		int inc_size = file->pos + len - file->size;
		int err = expand_file(file, inc_size);
		if (err < 0) {
			return 0;
		}
	}

	uint32_t nbytes = len;
	uint32_t total_write = 0;
	while (nbytes) {
		uint32_t cur_write = nbytes;
		uint32_t cluster_offset = file->pos % fat->cluster_byte_size;
		uint32_t start_sector = fat->data_start + (file->cblk - 2) * fat->sec_per_cluster;

		if (cluster_offset == 0 && nbytes == fat->cluster_byte_size) {
			int err = dev_write(fat->fs->dev_id, start_sector, buf, fat->sec_per_cluster);
			if (err < 0) {
				return total_write;
			}
			cur_write = fat->cluster_byte_size;
		} else {
			if (cluster_offset + cur_write > fat->cluster_byte_size) {
				cur_write = fat->cluster_byte_size - cluster_offset;
			}
			fat->current_sector = -1;
			int err = dev_read(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
			if (err < 0) {
				return total_write;
			}
			kernel_memcpy(fat->fat_buffer + cluster_offset, buf, cur_write);
			err = dev_write(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
			if (err < 0) {
				return total_write;
			}
		}

		buf += cur_write;
		nbytes -= cur_write;
		total_write += cur_write;

		file->size += cur_write;

		int err = move_file_pos(file, fat, cur_write, 1);
		if (err < 0) {
			return total_write;
		}
	}
	return total_write;
}

void fatfs_close(file_t *file) {
	if (file->mode == O_RDONLY) {
		return;
	}

	fat_t *fat = (fat_t *) file->fs->data;
	diritem_t *item = read_dir_entry(fat, file->index);
	if (item == (diritem_t *) 0) {
		return;
	}

	item->DIR_FileSize = file->size;
	item->DIR_FstClusHI = (uint16_t) (file->sblk >> 16);
	item->DIR_FstClusLO = (uint16_t) (file->sblk & 0xFFFF);
	write_dir_entry(fat, item, file->index);
}

int fatfs_seek(file_t *file, int offset, int whence) {
	if (whence != 0) {
		return -1;
	}

	fat_t *fat = (fat_t *) file->fs->data;
	cluster_t cur_cluster = file->sblk;
	uint32_t cur_pos = 0;
	uint32_t offset_to_move = offset;

	while (offset_to_move > 0) {
		uint32_t cur_offset = cur_pos % fat->cluster_byte_size;
		uint32_t cur_move = offset_to_move;
		if (cur_offset + cur_move < fat->cluster_byte_size) {
			cur_pos += cur_move;
			break;
		}

		cur_move = fat->cluster_byte_size - cur_offset;
		cur_pos += cur_move;
		offset_to_move -= cur_move;

		cur_cluster = cluster_get_next(fat, cur_cluster);
		if (!cluster_is_valid(cur_cluster)) {
			return -1;
		}
	}

	file->pos = cur_pos;
	file->cblk = cur_cluster;
	return 0;
}

int fatfs_stat(file_t *file, struct stat *st) {
	return -1;
}

int fatfs_opendir(struct _fs_t *fs, const char *path, DIR *dir) {
	dir->index = 0;
	return 0;
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

int fatfs_unlink(struct _fs_t *fs, const char *name) {
	fat_t *fat = (fat_t *) fs->data;
	for (int i = 0; i < fat->root_ent_cnt; ++i) {
		diritem_t *item = read_dir_entry(fat, i);
		if (item == (diritem_t *) 0) {
			return -1;
		}

		if (item->DIR_Name[0] == DIRITEM_NAME_END) {
			break;
		}

		if (item->DIR_Name[0] == DIRITEM_NAME_FREE) {
			continue;
		}

		if (diritem_name_match(item, name)) {
			int cluster = (item->DIR_FstClusHI << 16) | item->DIR_FstClusLO;
			cluster_free_chain(fat, cluster);

			kernel_memset(item, 0, sizeof(diritem_t));
			return write_dir_entry(fat, item, i);
		}
	}
	return -1;
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
		.unlink = fatfs_unlink,
};