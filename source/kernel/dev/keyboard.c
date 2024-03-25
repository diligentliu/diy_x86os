#include "dev/keyboard.h"
#include "cpu/irq.h"
#include "comm/cpu_instr.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "dev/tty.h"

static keyboard_state_t keyboard_state;    // 键盘状态

/**
 * 键盘映射表，分3类
 * normal是没有shift键按下，或者没有numlock按下时默认的键值
 * func是按下shift或者numlock按下时的键值
 * esc是以esc开头的的键值
 */
static const key_map_t map_table[256] = {
		[0x2] = {'1', '!'},
		[0x3] = {'2', '@'},
		[0x4] = {'3', '#'},
		[0x5] = {'4', '$'},
		[0x6] = {'5', '%'},
		[0x7] = {'6', '^'},
		[0x08] = {'7', '&'},
		[0x09] = {'8', '*'},
		[0x0A] = {'9', '('},
		[0x0B] = {'0', ')'},
		[0x0C] = {'-', '_'},
		[0x0D] = {'=', '+'},
		[0x0E] = {0x7F, 0x7F},
		[0x0F] = {'\t', '\t'},
		[0x10] = {'q', 'Q'},
		[0x11] = {'w', 'W'},
		[0x12] = {'e', 'E'},
		[0x13] = {'r', 'R'},
		[0x14] = {'t', 'T'},
		[0x15] = {'y', 'Y'},
		[0x16] = {'u', 'U'},
		[0x17] = {'i', 'I'},
		[0x18] = {'o', 'O'},
		[0x19] = {'p', 'P'},
		[0x1A] = {'[', '{'},
		[0x1B] = {']', '}'},
		[0x1C] = {'\n', '\n'},
		[0x1E] = {'a', 'A'},
		[0x1F] = {'s', 'B'},
		[0x20] = {'d', 'D'},
		[0x21] = {'f', 'F'},
		[0x22] = {'g', 'G'},
		[0x23] = {'h', 'H'},
		[0x24] = {'j', 'J'},
		[0x25] = {'k', 'K'},
		[0x26] = {'l', 'L'},
		[0x27] = {';', ':'},
		[0x28] = {'\'', '"'},
		[0x29] = {'`', '~'},
		[0x2B] = {'\\', '|'},
		[0x2C] = {'z', 'Z'},
		[0x2D] = {'x', 'X'},
		[0x2E] = {'c', 'C'},
		[0x2F] = {'v', 'V'},
		[0x30] = {'b', 'B'},
		[0x31] = {'n', 'N'},
		[0x32] = {'m', 'M'},
		[0x33] = {',', '<'},
		[0x34] = {'.', '>'},
		[0x35] = {'/', '?'},
		[0x39] = {' ', ' '},
};

static inline char get_key(uint8_t key_code) {
	return key_code & 0x7F;
}

static inline int is_make_code(uint8_t key_code) {
	return (key_code & 0x80) == 0;
}

void keyboard_init() {
	static int init_flag = 0;
	if (init_flag) {
		return;
	}
	init_flag = 1;
	kernel_memset(&keyboard_state, 0, sizeof(keyboard_state));
	irq_install(IRQ1_KEYBOARD, exception_handler_keyboard);
	irq_enable(IRQ1_KEYBOARD);
}

static void keyboard_wait_send_ready() {
	uint32_t time_out = 100000;
	while (time_out--) {
		if ((inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_SEND_FULL) == 0) {
			return;
		}
	}
}

static void keyboard_write(uint16_t port, uint8_t data) {
	keyboard_wait_send_ready();
	outb(port, data);
}

static void keyboard_wait_recv_ready() {
	uint32_t time_out = 100000;
	while (time_out--) {
		if (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_RECV_READY) {
			return;
		}
	}
}

static uint8_t keyboard_read() {
	keyboard_wait_recv_ready();
	return inb(KEYBOARD_DATA_PORT);
}

static void update_led_status() {
	uint8_t status = 0;

	status = (keyboard_state.caps_lock ? 1 : 0) << 0;
	keyboard_write(KEYBOARD_DATA_PORT, KBD_CMD_RW_LED);
	keyboard_write(KEYBOARD_DATA_PORT, status);
	keyboard_read();
}

static void do_fx_key(char key) {
	int minor = key - KEY_F1;
	// if (keyboard_state.left_ctrl_press || keyboard_state.right_ctrl_press) {
	// 	tty_select(minor);
	// }
	tty_select(minor);
}

static void do_normal_key(uint8_t scancode) {
	char key = get_key(scancode);
	int is_make = is_make_code(scancode);

	switch (key) {
		case KEY_RSHIFT:
			keyboard_state.right_shift_press = is_make;
			break;
		case KEY_LSHIFT:
			keyboard_state.left_shift_press = is_make;
			break;
		case KEY_CAPS:
			if (is_make) {
				keyboard_state.caps_lock = !keyboard_state.caps_lock;
				update_led_status();
			}
			break;
		case KEY_ALT:
			keyboard_state.left_alt_press = is_make;
			break;
		case KEY_CTRL:
			keyboard_state.left_ctrl_press = is_make;
			break;
			// 功能键：写入键盘缓冲区，由应用自行决定如何处理
		case KEY_F1:
		case KEY_F2:
		case KEY_F3:
		case KEY_F4:
		case KEY_F5:
		case KEY_F6:
		case KEY_F7:
		case KEY_F8:
			do_fx_key(key);
			break;
		case KEY_F9:
		case KEY_F10:
		case KEY_F11:
		case KEY_F12:
		case KEY_SCROLL_LOCK:
			break;
		default:
			if (is_make) {
				if (keyboard_state.left_shift_press || keyboard_state.right_shift_press) {
					key = map_table[key].func;
				} else {
					key = map_table[key].normal;
				}

				// 根据caps再进行一次字母的大小写转换
				if (keyboard_state.caps_lock) {
					if ((key >= 'A') && (key <= 'Z')) {
						// 大写转小写
						key = key - 'A' + 'a';
					} else if ((key >= 'a') && (key <= 'z')) {
						// 小写转大小
						key = key - 'a' + 'A';
					}
				}

				// log_printf("key : %c\n", key);
				tty_in(key);
			}
			break;
	}
}

static void do_e0_key(uint8_t raw_code) {
	int key = get_key(raw_code);            // 去掉最高位
	int is_make = is_make_code(raw_code);    // 按下或释放

	// E0开头，主要是HOME、END、光标移动等功能键
	// 设置一下光标位置，然后直接写入
	switch (key) {
		case KEY_CTRL:        // 右ctrl和左ctrl都是这个值
			keyboard_state.right_ctrl_press = is_make;  // 仅设置标志位
			break;
		case KEY_ALT:
			keyboard_state.right_alt_press = is_make;  // 仅设置标志位
			break;
	}
}

void do_handler_keyboard(exception_frame_t *frame) {
	static enum {
		NORMAL,                // 普通，无e0或e1
		BEGIN_E0,            // 收到e0字符
		BEGIN_E1,            // 收到e1字符
	} recv_state = NORMAL;

	// 检查是否有数据，无数据则退出
	uint8_t status = inb(KEYBOARD_STATUS_PORT);
	if (!(status & KEYBOARD_STATUS_RECV_READY)) {
		pic_send_eoi(IRQ1_KEYBOARD);
		return;
	}

	uint8_t raw_code = inb(KEYBOARD_DATA_PORT);
	pic_send_eoi(IRQ1_KEYBOARD);
	if (raw_code == KEY_E0) {
		// E0字符
		recv_state = BEGIN_E0;
	} else if (raw_code == KEY_E1) {
		// E1字符，不处理
		recv_state = BEGIN_E1;
	} else {
		switch (recv_state) {
			case NORMAL:
				do_normal_key(raw_code);
				break;
			case BEGIN_E0:
				do_e0_key(raw_code);
				recv_state = NORMAL;
				break;
			case BEGIN_E1:
				recv_state = NORMAL;
				break;
		}
	}
}