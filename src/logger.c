#include "common.h"
#include "logger.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/file.h>

static FILE* log_file = NULL;
static char log_file_path[512];

void init_logger(const char* process_name) {
    // Create logs directory if it doesn't exist
    struct stat st = {0};
    if (stat(LOG_DIR, &st) == -1) {
        mkdir(LOG_DIR, 0700);
    }
    
    // Create log file path
    snprintf(log_file_path, sizeof(log_file_path), "%s/%s_%d.log", 
             LOG_DIR, process_name, getpid());
    
    log_file = fopen(log_file_path, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        log_file = stderr;
    }
    
    // Set line buffering
    setvbuf(log_file, NULL, _IOLBF, 0);
}

void close_logger(void) {
    if (log_file && log_file != stderr && log_file != stdout) {
        fclose(log_file);
        log_file = NULL;
    }
}

void log_message(LogLevel level, const char* format, ...) {
    if (log_file == NULL) {
        log_file = stderr;
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    format_timestamp(now, timestamp, sizeof(timestamp));
    
    const char* level_str;
    switch (level) {
        case LOG_DEBUG: level_str = "DEBUG"; break;
        case LOG_INFO:  level_str = "INFO";  break;
        case LOG_WARN:  level_str = "WARN";  break;
        case LOG_ERROR: level_str = "ERROR"; break;
        default:        level_str = "INFO";  break;
    }
    
    // Format: [TIMESTAMP] [PID] [LEVEL] message
    fprintf(log_file, "[%s] [%d] [%s] ", timestamp, getpid(), level_str);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
}

const char* get_log_file_path(void) {
    return log_file_path;
}

