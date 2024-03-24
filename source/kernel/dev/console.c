#include "dev/console.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "dev/keyboard.h"

#define CONSOLE_NR TTY_NR
static console_t console_buf[CONSOLE_NR];
static int current_console = 0;

static uint16_t read_cursor_pos() {
	uint16_t pos;

	irq_state_t state = irq_enter_protection();
	outb(0x3D4, 0x0F);
	pos = inb(0x3D5);
	outb(0x3D4, 0x0E);
	pos |= inb(0x3D5) << 8;
	irq_leave_protection(state);
	return pos;
}

static void update_cursor_pos(console_t *console) {
	uint16_t pos = (console - console_buf) * (console->display_cols * console->display_rows);
	pos += console->cursor_row * console->display_cols + console->cursor_col;

	irq_state_t state = irq_enter_protection();
	outb(0x3D4, 0x0F);		// 写低地址
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);		// 写高地址
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
	irq_leave_protection(state);
}

static void erase_rows(console_t *con, int start, int end) {
	disp_char_t *disp_start = con->disp_base + start * con->display_cols;
	disp_char_t *disp_end = con->disp_base + (end + 1) * con->display_cols;
	while (disp_start < disp_end) {
		disp_start->c = ' ';
		disp_start->foreground = con->foreground;
		disp_start->background = con->background;
		++disp_start;
	}
}

static void scroll_up(console_t *con, int lines) {
	disp_char_t *dest = con->disp_base;
	disp_char_t *src = con->disp_base + con->display_cols * lines;
	uint32_t size = con->display_cols * (con->display_rows - lines) * sizeof(disp_char_t);
	kernel_memcpy(dest, src, size);

	erase_rows(con, con->display_rows - lines, con->display_rows - 1);
	con->cursor_row -= lines;
}

static void move_forward(console_t *con, int step) {
	for (int i = 0; i < step; ++i) {
		if (++con->cursor_col >= con->display_cols) {
			con->cursor_col = 0;
			if (++con->cursor_row >= con->display_rows) {
				scroll_up(con, 1);
			}
		}
	}
}

static void show_char(console_t *con, char c) {
	int offset = con->cursor_col + con->display_cols * con->cursor_row;
	disp_char_t *disp = con->disp_base + offset;
	disp->c = c;
	disp->foreground = con->foreground;
	disp->background = con->background;
	move_forward(con, 1);
}

static void console_clear(console_t *con) {
	int size = con->display_rows * con->display_cols;
	for (int i = 0; i < size; ++i) {
		con->disp_base[i].c = ' ';
		con->disp_base[i].foreground = con->foreground;
		con->disp_base[i].background = con->background;
	}
}

static void move_to_col0(console_t *con) {
	con->cursor_col = 0;
}

static void move_to_next_line(console_t *con) {
	if (++con->cursor_row >= con->display_rows) {
		scroll_up(con, 1);
	}
}

static int move_backward(console_t *con, int step) {
	int status = -1;

	for (int i = 0; i < step; ++i) {
		if (con->cursor_col > 0) {
			--con->cursor_col;
			status = 0;
		} else if (con->cursor_row > 0) {
			con->cursor_col = con->display_cols - 1;
			--con->cursor_row;
			status = 0;
		} else {
			status = -1;
			break;
		}
	}
	return status;
}

static void erase_backward(console_t *con) {
	if (move_backward(con, 1) == 0) {
		show_char(con, ' ');
		move_backward(con, 1);
	}
}

int console_init(int minor) {
	console_t *console = &console_buf[minor];

	console->display_rows = CONSOLE_ROW_MAX;
	console->display_cols = CONSOLE_COL_MAX;

	if (minor == 0) {
		uint16_t cursor_pos = read_cursor_pos();
		console->cursor_row = cursor_pos / console->display_cols;
		console->cursor_col = cursor_pos % console->display_cols;
	} else {
		console->cursor_row = console->cursor_col = 0;
		console_clear(console);
		// update_cursor_pos(console);
	}

	console->old_cursor_row = console->cursor_row;
	console->old_cursor_col = console->cursor_col;
	console->write_status = CONSOLE_WRITE_NORMAL;
	console->disp_base = (disp_char_t *) CONSOLE_DISP_ADDR +
						 minor * CONSOLE_COL_MAX * CONSOLE_ROW_MAX;
	console->foreground = COLOR_WHITE;
	console->background = COLOR_BLACK;
	// console_clear(console);
	mutex_init(&console->mutex);
	return 0;
}

static void write_normal(console_t *con, char c) {
	switch (c) {
		case ASCII_ESC:
			con->write_status = CONSOLE_WRITE_ESC;
			break;
		case 0x7F:
			erase_backward(con);
			break;
		case '\b':
			move_backward(con, 1);
			break;
		case '\r':
			move_to_col0(con);
			break;
		case '\n':
			// move_to_col0(con);
			move_to_next_line(con);
			break;
		default:
			if (c >= ' ' && c <= '~') {
				show_char(con, c);
			}
			break;
	}
}

static void save_cursor(console_t *con) {
	con->old_cursor_row = con->cursor_row;
	con->old_cursor_col = con->cursor_col;
}

static void restore_cursor(console_t *con) {
	con->cursor_row = con->old_cursor_row;
	con->cursor_col = con->old_cursor_col;
}

static void clear_esc_param(console_t *con) {
	kernel_memset(con->esc_param, 0, sizeof(con->esc_param));
	con->current_esc_param_index = 0;
}

static void set_font_style(console_t *con) {
	static const color_t color_table[] = {
			COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
			COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_GREY
	};
	for (int i = 0; i <= con->current_esc_param_index; ++i) {
		int param = con->esc_param[i];
		switch (param) {
			case 0:
				con->foreground = COLOR_WHITE;
				con->background = COLOR_BLACK;
				break;
			case 30 ... 37:
				con->foreground = color_table[param - 30];
				break;
			case 40 ... 47:
				con->background = color_table[param - 40];
				break;
			case 39:
				con->foreground = COLOR_WHITE;
				break;
			case 49:
				con->background = COLOR_BLACK;
				break;
			default:
				break;
		}
	}
}

static void move_left(console_t *con, int step) {
	step = step > 0 ? step : 1;
	int col = con->cursor_col - step;
	con->cursor_col = col >= 0 ? col : 0;
}

static void move_right(console_t *con, int step) {
	step = step > 0 ? step : 1;
	int col = con->cursor_col + step;
	con->cursor_col = col < con->display_cols ? col : con->display_cols - 1;
}

static void move_cursor(console_t *con) {
	con->cursor_row = con->esc_param[0];
	con->cursor_col = con->esc_param[1];
}

static void erase_in_display(console_t *con) {
	switch (con->esc_param[0]) {
		case 0:
			erase_rows(con, con->cursor_row, con->display_rows - 1);
			break;
		case 1:
			erase_rows(con, 0, con->cursor_row);
			break;
		case 2:
			erase_rows(con, 0, con->display_rows - 1);
			con->cursor_row = con->cursor_col = 0;
			break;
		default:
			break;
	}
}

static void write_esc_square(console_t *con, char c) {
	if (c >= '0' && c <= '9') {
		con->esc_param[con->current_esc_param_index] *= 10;
		con->esc_param[con->current_esc_param_index] += c - '0';
	} else if (c == ';' && con->current_esc_param_index < ESC_PARAM_MAX) {
		++con->current_esc_param_index;
	} else {
		switch (c) {
			case 'm':
				set_font_style(con);
				break;
			case 'D':
				move_left(con, con->esc_param[0]);
				break;
			case 'C':
				move_right(con, con->esc_param[0]);
				break;
			case 'H':
			case 'f':
				move_cursor(con);
				break;
			case 'J':
				erase_in_display(con);
				break;
			default:
				break;
		}
		con->write_status = CONSOLE_WRITE_NORMAL;
	}
}

static void write_esc(console_t *con, char c) {
	switch (c) {
		case '7':
			save_cursor(con);
			con->write_status = CONSOLE_WRITE_NORMAL;
			break;
		case '8':
			restore_cursor(con);
			con->write_status = CONSOLE_WRITE_NORMAL;
			break;
		case '[':
			clear_esc_param(con);
			con->write_status = CONSOLE_WRITE_SQUARE;
			break;
		default:
			con->write_status = CONSOLE_WRITE_NORMAL;
			break;
	}
}

int console_write(tty_t *tty) {
	console_t *console = &console_buf[tty->console];
	int len = 0;

	mutex_lock(&console->mutex);
	do {
		char c;
		int err = tty_fifo_get(&tty->ofifo, &c);
		if (err < 0) {
			break;
		}
		sem_v(&tty->osem);

		switch (console->write_status) {
			case CONSOLE_WRITE_NORMAL:
				write_normal(console, c);
				break;
			case CONSOLE_WRITE_ESC:
				write_esc(console, c);
				break;
			case CONSOLE_WRITE_SQUARE:
				write_esc_square(console, c);
				break;
			default:
				break;
		}
		++len;
	} while (1);

	mutex_unlock(&console->mutex);
	if (tty->console == current_console) {
		update_cursor_pos(console);
	}
	return len;
}

void console_close(int console) {
	// do nothing
}

void console_select(int minor) {
	console_t *console = &console_buf[minor];
	if (console->disp_base == 0) {
		console_init(minor);
	}

	uint16_t pos = minor * console->display_cols * console->display_rows;

	outb(0x3D4, 0xC);		// 写高地址
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
	outb(0x3D4, 0xD);		// 写低地址
	outb(0x3D5, (uint8_t) (pos & 0xFF));

	// 更新光标到当前屏幕
	current_console = minor;
	update_cursor_pos(console);

	// 测试代码
	// char num = minor + '0';
	// show_char(console, num);
}