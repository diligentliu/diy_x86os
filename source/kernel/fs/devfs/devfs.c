#include "fs/devfs/devfs.h"
#include "dev/dev.h"
#include "tools/klib.h"
#include "tools/log.h"

static devfs_type_t dev_type_list[] = {
		{
				.name = "tty",
				.dev_type = DEV_TYPE_TTY,
				.file_type = FILE_TYPE_TTY,
		}
};

int devfs_mount(struct _fs_t *fs, int major, int minor) {
	fs->type = FS_TYPE_DEV;
}

void devfs_unmount(struct _fs_t *fs) {

}

int devfs_open(struct _fs_t *fs, const char *path, file_t *file) {
	for (int i = 0; i < sizeof(dev_type_list) / sizeof(devfs_type_t); ++i) {
		devfs_type_t *type = &dev_type_list[i];
		int type_name_len = kernel_strlen(type->name);
		if (kernel_strncmp(path, type->name, type_name_len) == 0) {
			int minor = 0;
			if (kernel_strlen(path) > type_name_len &&
			    path_to_num(path + type_name_len, &minor) < 0) {
				log_printf("get device id failed\n");
				break;
			}
			int dev_id = dev_open(type->dev_type, minor, (void *) 0);
			if (dev_id < 0) {
				log_printf("open device failed: %s\n", path);
				break;
			}

			file->dev_id = dev_id;
			file->fs = fs;
			file->type = type->file_type;
			file->pos = 0;
			file->size = 0;
			return 0;
		}
	}
	return -1;
}

int devfs_read(void *buf, int len, file_t *file) {
	return dev_read(file->dev_id, file->pos, buf, len);
}

int devfs_write(char *buf, int len, file_t *file) {
	return dev_write(file->dev_id, file->pos, buf, len);
}

void devfs_close(file_t *file) {
	dev_close(file->dev_id);
}

int devfs_seek(file_t *file, int offset, int whence) {
	return -1;
}

int devfs_stat(file_t *file, struct stat *st) {
	return -1;
}

int devfs_ioctl(file_t *file, int cmd, int arg0, int arg1) {
	return dev_control(file->dev_id, cmd, arg0, arg1);
}

fs_op_t devfs_op = {
		.mount = devfs_mount,
		.unmount = devfs_unmount,
		.open = devfs_open,
		.read = devfs_read,
		.write = devfs_write,
		.close = devfs_close,
		.seek = devfs_seek,
		.stat = devfs_stat,
		.ioctl = devfs_ioctl,
};