#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// 日志管理结构体
typedef struct {
    FILE *log_file;
    int log_to_console;  // 是否同时输出到控制台
    int enabled;
} Logger;

// 全局日志实例
extern Logger g_logger;

/**
 * 初始化日志系统
 * @param filename 日志文件路径
 * @param log_to_console 是否同时输出到控制台 (1=是, 0=否)
 * @return 0=成功, -1=失败
 */
int logger_init(const char *filename, int log_to_console);

/**
 * 关闭日志系统
 */
void logger_close(void);

/**
 * 写入日志(替代printf)
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void log_printf(const char *format, ...);

/**
 * 刷新日志缓冲区
 */
void logger_flush(void);

#endif // LOGGER_H
