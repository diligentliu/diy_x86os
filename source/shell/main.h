#ifndef OS_MAIN_H
#define OS_MAIN_H

#define CLI_INPUT_SIZE          1024
#define	CLI_MAX_ARG_COUNT		10

#define ESC_CMD2(Pn, cmd)		    "\x1b["#Pn#cmd
#define	ESC_COLOR_ERROR			    ESC_CMD2(31, m)	        // 红色错误
#define	ESC_COLOR_DEFAULT		    ESC_CMD2(39, m)	        // 默认颜色
#define ESC_CLEAR_SCREEN		    ESC_CMD2(2, J)	        // 擦除整屏幕
#define	ESC_MOVE_CURSOR(row, col)   "\x1b["#row";"#col"H"	// 移动光标到指定位置

typedef struct _cli_cmd_t {
	const char *name;
	const char *usage;
	int (*do_func)(int argc, char *argv[]);
} cli_cmd_t;

typedef struct _cli_t {
	char curr_input[CLI_INPUT_SIZE];
	const cli_cmd_t *cmd_start;
	const cli_cmd_t *cmd_end;
	const char* promot;
} cli_t;

#endif //OS_MAIN_H
