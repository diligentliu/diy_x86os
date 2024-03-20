/**
 * 进程启动C部分代码
 */
#include "lib_syscall.h"

int main(int argc, char **argv);

extern uint8_t __bss_start__[], __bss_end__[];
/**
 * @brief 应用的初始化，C部分
 */
void cstart(int argc, char **argv) {
	// 清空 bss 区
	uint8_t *start = __bss_start__;
	while (start < __bss_end__) {
		*start++ = 0;
	}
	main(argc, argv);
}