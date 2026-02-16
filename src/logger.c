#include "../inc/logger.h"
#include <stdarg.h>
#include <string.h>

// 全局日志实例
Logger g_logger = {NULL, 1, 0};

// 初始化日志系统
int logger_init(const char *filename, int log_to_console) {
    if (filename == NULL) {
        // 只输出到控制台
        g_logger.log_file = NULL;
        g_logger.log_to_console = 1;
        g_logger.enabled = 1;
        return 0;
    }
    
    g_logger.log_file = fopen(filename, "w");
    if (!g_logger.log_file) {
        printf("Warning: Cannot create log file: %s, logging to console only\n", filename);
        g_logger.log_to_console = 1;
        g_logger.enabled = 1;
        return -1;
    }
    
    g_logger.log_to_console = log_to_console;
    g_logger.enabled = 1;
    
    printf("Log file created: %s\n", filename);
    return 0;
}

// 关闭日志系统
void logger_close(void) {
    if (g_logger.log_file) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }
    g_logger.enabled = 0;
}

// 写入日志
void log_printf(const char *format, ...) {
    if (!g_logger.enabled) return;
    
    va_list args;
    
    // 输出到控制台
    if (g_logger.log_to_console) {
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    
    // 输出到文件
    if (g_logger.log_file) {
        va_start(args, format);
        vfprintf(g_logger.log_file, format, args);
        va_end(args);
    }
}

// 刷新缓冲区
void logger_flush(void) {
    if (g_logger.log_file) {
        fflush(g_logger.log_file);
    }
    fflush(stdout);
}
