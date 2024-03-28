#ifndef OS_TTY_H
#define OS_TTY_H

#include "ipc/sem.h"
#include "dev.h"

#define TTY_NR                       8          // 最大支持的tty设备数量
#define TTY_IBUF_SIZE                512        // tty输入缓存大小
#define TTY_OBUF_SIZE                512        // tty输出缓存大小

#define TTY_CMD_ECHO				 1          // 回显

typedef struct _tty_fifo_t {
	char *buf;
	int size;                // 最大字节数
	int read, write;        // 当前读写位置
	int count;                // 当前已有的数据量
} tty_fifo_t;

#define TTY_OCRLF            (1 << 0)        // 输出是否将 \n 转换成 \r\n
#define TTY_IECHO            (1 << 2)        // 输入是否回显
#define TTY_INCLR            (1 << 0)        // 输入是否将 \n 转换成 \r\n
/**
 * tty设备
 */
typedef struct _tty_t {
	char obuf[TTY_OBUF_SIZE];
	tty_fifo_t ofifo;                // 输出队列
	sem_t osem;                        // 输出信号量
	int oflags;                        // 输出标志
	char ibuf[TTY_IBUF_SIZE];
	tty_fifo_t ififo;                // 输入处理后的队列
	sem_t isem;                        // 输入信号量
	int iflags;                        // 输入标志
	int console;                    // 控制台
} tty_t;

void tty_fifo_init(tty_fifo_t *fifo, char *buf, int size);
int tty_fifo_put(tty_fifo_t *fifo, char c);
int tty_fifo_get(tty_fifo_t *fifo, char *c);

int tty_open(device_t *dev);
int tty_read(device_t *dev, int addr, char *buf, int size);
int tty_write(device_t *dev, int addr, const char *buf, int size);
int tty_control(device_t *dev, int cmd, int arg0, int arg1);
void tty_close(device_t *dev);

void tty_in(char c);

void tty_select(int minor);

#endif //OS_TTY_H
