# diy_x86os

## diy_x86os 项目内容描述

这个项目是一个自制 x86 操作系统的实现。它的目标是通过编写自己的操作系统内核，了解操作系统的基本原理和实现细节。项目参考了李述铜老师的相关课程

项目特点：
- 使用汇编语言和 C 语言编写
- 实现基本的内存管理和进程调度
- 支持基本的输入输出功能
- 提供简单的 shell 交互

该项目适合对操作系统感兴趣的开发者，希望深入了解操作系统内部工作原理的人。

## 操作系统所具有的功能有

- [x] 引导程序, 从磁盘加载操作系统内核
- [x] 加载器 loader, 加载内核到内存
- [x] 中断与异常处理
- [x] 日志与输出
- [x] 进程的管理, 实现进程的创建、调度和切换
- [x] 进程间同步与互斥
- [x] 虚拟内存实现
- [x] 隔离操作系统和用户程序
- [x] 系统调用, 包括 fork、exec、yield、dup 系统调用
- [x] shell 交互功能
- [x] 文件系统加载

## 项目结构

```
os
├── CMakeLists.txt
├── README.md
├── scripts
├── newlib
├── source
    ├── applib
    │   ├── CMakeLists.txt
    │   ├── crt0.S
    │   ├── cstart.c
    │   ├── lib_syscall.c
    │   └── lib_syscall.h
    ├── boot
    │   ├── CMakeLists.txt
    │   ├── boot.c
    │   ├── boot.h
    │   └── start.S
    ├── comm
    │   ├── boot_info.h
    │   ├── cpu_instr.h
    │   ├── elf.h
    │   └── types.h
    ├── init
    │   ├── CMakeLists.txt
    │   ├── link.lds
    │   ├── main.c
    │   └── main.h
    ├── kernel
    │   ├── CMakeLists.txt
    │   ├── core
    │   │   ├── memory.c
    │   │   ├── syscall.c
    │   │   └── task.c
    │   ├── cpu
    │   │   ├── cpu.c
    │   │   ├── irq.c
    │   │   └── mmu.c
    │   ├── dev
    │   │   ├── console.c
    │   │   ├── dev.c
    │   │   ├── disk.c
    │   │   ├── keyboard.c
    │   │   ├── time.c
    │   │   └── tty.c
    │   ├── fs
    │   │   ├── devfs
    │   │   │   └── devfs.c
    │   │   ├── fatfs
    │   │   │   └── fatfs.c
    │   │   ├── file.c
    │   │   └── fs.c
    │   ├── include
    │   │   ├── core
    │   │   │   ├── memory.h
    │   │   │   ├── syscall.h
    │   │   │   └── task.h
    │   │   ├── cpu
    │   │   │   ├── cpu.h
    │   │   │   ├── irq.h
    │   │   │   └── mmu.h
    │   │   ├── dev
    │   │   │   ├── console.h
    │   │   │   ├── dev.h
    │   │   │   ├── disk.h
    │   │   │   ├── keyboard.h
    │   │   │   ├── time.h
    │   │   │   └── tty.h
    │   │   ├── fs
    │   │   │   ├── devfs
    │   │   │   │   └── devfs.h
    │   │   │   ├── fatfs
    │   │   │   │   └── fatfs.h
    │   │   │   ├── file.h
    │   │   │   └── fs.h
    │   │   ├── ipc
    │   │   │   ├── mutex.h
    │   │   │   └── sem.h
    │   │   ├── os_cfg.h
    │   │   └── tools
    │   │       ├── bitmap.h
    │   │       ├── klib.h
    │   │       ├── list.h
    │   │       └── log.h
    │   ├── init
    │   │   ├── first_task.c
    │   │   ├── first_task_entry.S
    │   │   ├── init.c
    │   │   ├── init.h
    │   │   ├── lib_syscall.c
    │   │   └── start.S
    │   ├── ipc
    │   │   ├── mutex.c
    │   │   └── sem.c
    │   ├── kernel.lds
    │   └── tools
    │       ├── bitmap.c
    │       ├── klib.c
    │       ├── list.c
    │       └── log.c
    ├── loader
    │   ├── CMakeLists.txt
    │   ├── loader.h
    │   ├── loader_16.c
    │   ├── loader_32.c
    │   └── start.S
    ├── loop
    │   ├── CMakeLists.txt
    │   ├── link.lds
    │   ├── main.c
    │   └── main.h
    └── shell
        ├── CMakeLists.txt
        ├── link.lds
        ├── main.c
        └── main.h

```

