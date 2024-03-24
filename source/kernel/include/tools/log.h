/**
 * 日志输出
 */
#ifndef OS_LOG_H
#define OS_LOG_H

void log_init();
void log_printf(const char *fmt, ...);

#endif // OS_LOG_H