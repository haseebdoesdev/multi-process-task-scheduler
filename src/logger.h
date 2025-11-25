#ifndef LOGGER_H
#define LOGGER_H

#include "common.h"

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3
} LogLevel;

void init_logger(const char* process_name);
void close_logger(void);
void log_message(LogLevel level, const char* format, ...);
const char* get_log_file_path(void);

// Convenience macros
#define LOG_DEBUG_F(...) log_message(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO_F(...)  log_message(LOG_INFO, __VA_ARGS__)
#define LOG_WARN_F(...)  log_message(LOG_WARN, __VA_ARGS__)
#define LOG_ERROR_F(...) log_message(LOG_ERROR, __VA_ARGS__)

#endif // LOGGER_H

