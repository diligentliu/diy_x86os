if [ -f "disk1.vhd" ]; then
    cp disk1.vhd disk1.dmg
fi

if [ -f "disk2.vhd" ]; then
    cp disk2.vhd disk2.dmg
fi

export DISK1_NAME=disk1.dmg

# 写boot区，定位到磁盘开头，写1个块：512字节
dd if=boot.bin of=$DISK1_NAME bs=512 conv=notrunc count=1

# 写loader区，定位到磁盘第2个块，写1个块：512字节
dd if=loader.bin of=$DISK1_NAME bs=512 conv=notrunc seek=1

# 写kernel区，定位到磁盘第100个块
dd if=kernel.elf of=$DISK1_NAME bs=512 conv=notrunc seek=100

# 写应用程序init，临时使用
# dd if=init.elf of=$DISK1_NAME bs=512 conv=notrunc seek=5000
# dd if=shell.elf of=$DISK1_NAME bs=512 conv=notrunc seek=5000

# 写应用程序，使用系统的挂载命令
# 设置变量
DISK2_NAME=disk2.dmg
TARGET_PATH=mp

# 检查挂载点目录是否存在，如果存在，则先卸载并删除
if [ -d "$TARGET_PATH" ]; then
    echo "$TARGET_PATH exists, attempting to unmount and remove..."
    hdiutil detach "$TARGET_PATH" -force
    rm -rf "$TARGET_PATH"
fi

# 创建挂载点目录
mkdir -p "$TARGET_PATH"

# 挂载磁盘镜像到目标挂载点
echo "Mounting $DISK2_NAME to $TARGET_PATH..."
hdiutil attach "$DISK2_NAME" -mountpoint "$TARGET_PATH"

# 复制文件到挂载点
# 由于你注释了两个复制操作，我将只保留未注释的那一个
# 如果需要复制 init.elf 或 loop.elf，可以取消相应行的注释
# cp -v init.elf "$TARGET_PATH/init"
# cp -v shell.elf "$TARGET_PATH"
# cp -v loop.elf "$TARGET_PATH/loop"
cp -v *.elf "$TARGET_PATH"

# 卸载磁盘镜像
echo "Unmounting $TARGET_PATH..."
hdiutil detach "$TARGET_PATH" -force

# 删除挂载点目录（可选，如果你希望保留空目录则可以注释掉这一行）
rm -rf "$TARGET_PATH"

