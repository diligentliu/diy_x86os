/**
 * 进程启动C部分代码
 */
#include "lib_syscall.h"

int main(int argc, char **argv);

/**
 * @brief 应用的初始化，C部分
 */
void cstart(int argc, char **argv) {
	main(argc, argv);
}