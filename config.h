#ifndef CONFIG_H
#define CONFIG_H

// Queue and Process Configuration
#define MAX_TASKS 100
#define NUM_WORKERS 3
#define MAX_THREADS_PER_WORKER 4

// IPC Keys (using ftok or fixed keys)
#define SHM_KEY 0x12345678
#define SEM_KEY 0x87654321
#define MSG_KEY 0xABCDEF00

// Paths
#define LOG_DIR "logs"
#define PID_FILE "scheduler.pid"
#define TASK_PIPE_PATH "/tmp/task_scheduler_pipe"

// Timeout values (in seconds)
#define WORKER_CHECK_INTERVAL 5
#define MONITOR_REFRESH_INTERVAL 2
#define CLEANUP_INTERVAL 60  // Clean up completed tasks every 60 seconds
#define COMPLETED_TASK_MAX_AGE 300  // Remove completed tasks older than 5 minutes
#define TASK_TIMEOUT_CHECK_INTERVAL 2  // Check for task timeouts every 2 seconds
#define DEFAULT_TASK_TIMEOUT_SECONDS 300  // Default timeout: 5 minutes (0 = no timeout)
#define MAX_TASK_RETRIES 3  // Maximum number of retries for failed tasks

#endif // CONFIG_H

