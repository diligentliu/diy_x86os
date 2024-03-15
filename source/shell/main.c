#include "lib_syscall.h"

int main(int argc, char *argv[]) {
	while (1) {
		for (int i = 0; i < argc; ++i) {
			print_msg("argv = %s", argv[i]);
		}
		msleep(1000);
	}
	return 0;
}