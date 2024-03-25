#ifndef OS_CONSOLE_H
#define OS_CONSOLE_H

#include "comm/types.h"
#include "tty.h"
#include "ipc/mutex.h"

#define CONSOLE_DISP_ADDR       0xb8000
#define CONSOLE_DISP_END        (CONSOLE_DISP_ADDR + 32 * 1024)
#define CONSOLE_ROW_MAX         25
#define CONSOLE_COL_MAX         80

#define ESC_PARAM_MAX           10

typedef enum _color_t {
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_YELLOW = 14,
	COLOR_WHITE = 15
} color_t;

typedef union _disp_char_t {
	struct {
		char c;
		char foreground: 4;
		char background: 3;
	};
	uint16_t data;
} disp_char_t;

typedef struct _console_t {
	enum {
		CONSOLE_WRITE_NORMAL = 0,
		CONSOLE_WRITE_ESC = 1,
		CONSOLE_WRITE_SQUARE = 2,
	} write_status;
	disp_char_t *disp_base;
	int cursor_col, cursor_row;
	int display_rows, display_cols;
	color_t foreground, background;
	int old_cursor_row, old_cursor_col;
	int esc_param[ESC_PARAM_MAX];
	int current_esc_param_index;

	mutex_t mutex;
} console_t;

int console_init(int minor);
int console_write(tty_t *tty);
void console_close(int console);
void console_select(int minor);

#endif //OS_CONSOLE_H
