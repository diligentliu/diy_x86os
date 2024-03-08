@REM ÊÊÓÃÓÚwindows
start qemu-system-i386  -m 128M -s -S -serial stdio -drive file=disk1.vhd,index=0,media=disk,format=raw -drive file=disk2.vhd,index=2,media=disk,format=raw -d pcall,page,mmu,cpu_reset,guest_errors,page,trace:ps2_keyboard_set_translation
