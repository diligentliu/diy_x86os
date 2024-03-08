set DISK1_NAME=disk1.vhd

@REM 写boot区，定位到磁盘开头，写1个块：512字节
dd if=boot.bin of=%DISK1_NAME% bs=512 conv=notrunc count=1

@REM 写loader区，定位到磁盘第2个块，写1个块：512字节
dd if=loader.bin of=%DISK1_NAME% bs=512 conv=notrunc seek=1

@REM 写kernel区，定位到磁盘第100个块
dd if=kernel.elf of=%DISK1_NAME% bs=512 conv=notrunc seek=100

@REM  写应用程序init，临时使用
@REM dd if=init.elf of=%DISK1_NAME% bs=512 conv=notrunc seek=5000
@REM dd if=shell.elf of=%DISK1_NAME% bs=512 conv=notrunc seek=5000

@REM 写应用程序，使用系统的挂载命令
@REM set DISK2_NAME=disk2.vhd
@REM set TARGET_PATH=k
@REM echo select vdisk file="%cd%\%DISK2_NAME%" >a.txt
@REM echo attach vdisk >>a.txt
@REM echo select partition 1 >> a.txt
@REM echo assign letter=%TARGET_PATH% >> a.txt
@REM diskpart /s a.txt
@REM del a.txt

@REM 复制应用程序
@REM copy /Y init.elf %TARGET_PATH%:\init
@REM copy /Y shell.elf %TARGET_PATH%:\shell.elf
@REM copy /Y loop %TARGET_PATH%:\loop

@REM echo select vdisk file="%cd%\%DISK2_NAME%" >a.txt
@REM echo detach vdisk >>a.txt
@REM diskpart /s a.txt
@REM del a.txt
