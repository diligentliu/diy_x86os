#include "dev/tty.h"
#include "dev/dev.h"
#include "tools/log.h"
#include "dev/keyboard.h"
#include "dev/console.h"

static tty_t tty_devs[TTY_NR];
static int current_tty = 0;

void tty_fifo_init(tty_fifo_t *fifo, char *buf, int size) {
	fifo->buf = buf;
	fifo->size = size;
	fifo->read = fifo->write = fifo->count = 0;
}

int tty_fifo_put(tty_fifo_t *fifo, char c) {
	irq_state_t state = irq_enter_protection();
	if (fifo->count >= fifo->size) {
		irq_leave_protection(state);
		return -1;
	}
	fifo->buf[fifo->write++] = c;
	if (fifo->write >= fifo->size) {
		fifo->write = 0;
	}
	++fifo->count;
	irq_leave_protection(state);
	return 0;
}

int tty_fifo_get(tty_fifo_t *fifo, char *c) {
	irq_state_t state = irq_enter_protection();
	if (fifo->count <= 0) {
		irq_leave_protection(state);
		return -1;
	}
	*c = fifo->buf[fifo->read++];
	if (fifo->read >= fifo->size) {
		fifo->read = 0;
	}
	--fifo->count;
	irq_leave_protection(state);
	return 0;
}

static inline tty_t *get_tty(device_t *dev) {
	int tty = dev->minor;
	if (tty < 0 || tty >= TTY_NR || !dev->open_count) {
		log_printf("get_tty: invalid tty %d\n", tty);
		return (tty_t *) 0;
	}
	return tty_devs + tty;
}

int tty_open(device_t *dev) {
	int minor = dev->minor;
	if (minor < 0 || minor >= TTY_NR) {
		log_printf("tty_open: invalid minor %d\n", minor);
		return -1;
	}
	tty_t *tty = tty_devs + minor;
	tty_fifo_init(&tty->ififo, tty->ibuf, TTY_IBUF_SIZE);
	sem_init(&tty->isem, 0);
	tty->iflags = TTY_INCLR | TTY_IECHO;
	tty_fifo_init(&tty->ofifo, tty->obuf, TTY_OBUF_SIZE);
	sem_init(&tty->osem, TTY_OBUF_SIZE);
	tty->oflags = TTY_OCRLF;
	tty->console = minor;
	keyboard_init();
	console_init(minor);
	return 0;
}

int tty_read(device_t *dev, int addr, char *buf, int size) {
	if (size < 0) {
		return -1;
	}

	tty_t *tty = get_tty(dev);
	char *p_buf = buf;
	int len = 0;
	while (len < size) {
		sem_p(&tty->isem);
		char c;
		tty_fifo_get(&tty->ififo, &c);
		switch (c) {
			case ASCII_DEL:
				if (len == 0) {
					continue;
				}
				--len;
				--p_buf;
				break;
			case '\n':
				if ((tty->iflags & TTY_INCLR) && len < size - 1) {
					*p_buf++ = '\r';
					++len;
				}
				*p_buf++ = '\n';
				++len;
				break;
			default:
				*p_buf++ = c;
				++len;
				break;
		}
		if (tty->iflags & TTY_IECHO) {
			tty_write(dev, 0, &c, 1);
		}
		if (c == '\r' || c == '\n') {
			break;
		}
	}

	return len;
}

int tty_write(device_t *dev, int addr, const char *buf, int size) {
	if (size < 0) {
		return -1;
	}

	tty_t *tty = get_tty(dev);
	int len = 0;
	while (size) {
		char c = *buf++;

		if (c == '\n' && (tty->oflags & TTY_OCRLF)) {
			sem_p(&tty->osem);
			int err = tty_fifo_put(&tty->ofifo, '\r');
			if (err < 0) {
				break;
			}
		}

		sem_p(&tty->osem);
		int err = tty_fifo_put(&tty->ofifo, c);
		if (err < 0) {
			break;
		}
		++len;
		--size;

		console_write(tty);
	}

	return len;
}

int tty_control(device_t *dev, int cmd, int arg0, int arg1) {
	return 0;
}

void tty_close(device_t *dev) {

}

void tty_in(char c) {
	tty_t *tty = tty_devs + current_tty;
	if (sem_count(&tty->isem) >= TTY_IBUF_SIZE) {
		return;
	}
	tty_fifo_put(&tty->ififo, c);
	sem_v(&tty->isem);
}

void tty_select(int minor) {
	if (minor != current_tty) {
		console_select(minor);
		current_tty = minor;
	}
}

dev_desc_t dev_tty_desc = {
		.name = "tty",
		.major = DEV_TYPE_TTY,
		.open = tty_open,
		.read = tty_read,
		.write = tty_write,
		.control = tty_control,
		.close = tty_close,
};