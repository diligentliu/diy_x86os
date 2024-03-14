/**
 * os配置
 */
#ifndef OS_OS_CFG_H
#define OS_OS_CFG_H

#define GDT_TABLE_SIZE              256               // GDT表项数量
#define KERNEL_SELECTOR_CS          (1 * 8)           // 内核代码段描述符
#define KERNEL_SELECTOR_DS          (2 * 8)           // 内核数据段描述符
#define SELECTOR_SYSCALL            (3 * 8)
#define KERNEL_STACK_SIZE           (8 * 1024)        // 内核栈

#define OS_TICKS_MS                 10                // 每毫秒的时钟数

#define IDLE_TASK_STACK_SIZE        1024              // 空闲任务栈大小

#define OS_VERSION                  "0.0.1"           // OS版本号

#endif //OS_OS_CFG_H
