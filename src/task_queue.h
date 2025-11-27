#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include "common.h"

// Task Structure
typedef struct {
    int id;
    char name[MAX_TASK_NAME_LEN];
    Priority priority;
    TaskStatus status;
    time_t creation_time;
    time_t start_time;
    time_t end_time;
    unsigned int execution_time_ms;
    unsigned int timeout_seconds;  // Task timeout (0 = no timeout)
    int retry_count;  // Number of retries attempted
    int worker_id;
    pthread_t thread_id;
} Task;

// Shared Memory Structure
typedef struct {
    Task tasks[MAX_TASKS];
    int size;
    int capacity;
    int next_task_id;
    
    // Global counters
    int total_tasks;
    int completed_tasks;
    int failed_tasks;
    
    // Synchronization
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    
    // Worker status
    pid_t scheduler_pid;
    int num_active_workers;
    
    // Shutdown flag
    int shutdown_flag;
} TaskQueue;

// Function prototypes
int init_shared_memory(void);
TaskQueue* attach_shared_memory(int shm_id);
void detach_shared_memory(TaskQueue* queue);
void destroy_shared_memory(int shm_id);

int enqueue_task(TaskQueue* queue, const char* name, Priority priority, unsigned int execution_time_ms, unsigned int timeout_seconds);
int dequeue_task(TaskQueue* queue, Task* task);
int update_task_status(TaskQueue* queue, int task_id, TaskStatus new_status, time_t* time_field);
Task* find_task_by_id(TaskQueue* queue, int task_id);

int is_queue_full(TaskQueue* queue);
int is_queue_empty(TaskQueue* queue);
int get_pending_task_count(TaskQueue* queue);  // Requires mutex locked
int get_running_task_count(TaskQueue* queue);  // Requires mutex locked
int get_pending_task_count_safe(TaskQueue* queue);  // Thread-safe, locks internally
int get_running_task_count_safe(TaskQueue* queue);  // Thread-safe, locks internally

// Cleanup function for completed tasks
int remove_completed_tasks(TaskQueue* queue, int max_age_seconds);

// Task recovery function - resets orphaned tasks from crashed workers
int recover_orphaned_tasks(TaskQueue* queue, int dead_worker_id);

// Task timeout function - checks and handles timed-out tasks
int check_and_handle_timeouts(TaskQueue* queue);

#endif // TASK_QUEUE_H

