#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

// Include configuration
#include "../config.h"

// Task Priority Levels
typedef enum {
    PRIORITY_HIGH = 0,
    PRIORITY_MEDIUM = 1,
    PRIORITY_LOW = 2
} Priority;

// Task Status
typedef enum {
    STATUS_PENDING = 0,
    STATUS_RUNNING = 1,
    STATUS_COMPLETED = 2,
    STATUS_FAILED = 3
} TaskStatus;

// Utility macros
#define MAX_TASK_NAME_LEN 256
#define MAX_LOG_MESSAGE_LEN 512

// Priority string conversion
const char* priority_to_string(Priority p);
const char* status_to_string(TaskStatus s);

// Time utilities
time_t get_current_time(void);
void format_timestamp(time_t t, char* buffer, size_t size);

#endif // COMMON_H

