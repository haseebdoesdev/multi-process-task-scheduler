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

// Scheduling Algorithms
typedef enum {
    SCHED_ALGORITHM_PRIORITY = 0,  // Priority-based (default)
    SCHED_ALGORITHM_EDF = 1,       // Earliest Deadline First
    SCHED_ALGORITHM_MLFQ = 2,      // Multi-Level Feedback Queue
    SCHED_ALGORITHM_GANG = 3,      // Gang Scheduling
    SCHED_ALGORITHM_ROUND_ROBIN = 4, // Round Robin (time-sliced)
    SCHED_ALGORITHM_SJF = 5,       // Shortest Job First
    SCHED_ALGORITHM_FIFO = 6,      // First In First Out / FCFS
    SCHED_ALGORITHM_LOTTERY = 7,   // Lottery Scheduling
    SCHED_ALGORITHM_SRTF = 8       // Shortest Remaining Time First
} SchedulingAlgorithm;

// Utility macros
#define MAX_TASK_NAME_LEN 256
#define MAX_LOG_MESSAGE_LEN 512

// Priority string conversion
const char* priority_to_string(Priority p);
const char* status_to_string(TaskStatus s);
const char* algorithm_to_string(SchedulingAlgorithm alg);

// Time utilities
time_t get_current_time(void);
void format_timestamp(time_t t, char* buffer, size_t size);

#endif // COMMON_H

