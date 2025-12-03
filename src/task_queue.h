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
    int worker_id;
    pthread_t thread_id;
    
    // Advanced scheduling fields
    time_t deadline_time;        // For EDF: deadline for task completion
    int gang_id;                 // For Gang scheduling: group identifier (-1 if not in gang)
    unsigned int cpu_time_used;  // For MLFQ: total CPU time consumed (in ms)
    Priority current_mlfq_level; // For MLFQ: current priority level (can change)
    time_t mlfq_level_start;     // For MLFQ: when entered current level
    unsigned int lottery_tickets; // For Lottery Scheduling: number of tickets/weight
    unsigned int remaining_time_ms; // For SRTF: remaining execution time
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
    
    // Scheduling algorithm
    SchedulingAlgorithm algorithm;
    
    // MLFQ configuration
    unsigned int mlfq_time_slice_ms;  // Time quantum per level
    unsigned int mlfq_max_cpu_time_ms; // Max CPU time before priority demotion
    
    // Round Robin configuration
    int rr_last_index;                // Last scheduled task index (for Round Robin)
    unsigned int rr_time_quantum_ms;  // Time quantum for Round Robin
    
    // CPU affinity tracking
    int num_cpu_cores;
} TaskQueue;

// Function prototypes
int init_shared_memory(void);
TaskQueue* attach_shared_memory(int shm_id);
void detach_shared_memory(TaskQueue* queue);
void destroy_shared_memory(int shm_id);

int enqueue_task(TaskQueue* queue, const char* name, Priority priority, unsigned int execution_time_ms);
int enqueue_task_advanced(TaskQueue* queue, const char* name, Priority priority, unsigned int execution_time_ms, 
                          time_t deadline_time, int gang_id);
int dequeue_task(TaskQueue* queue, Task* task);
int dequeue_task_algorithm(TaskQueue* queue, Task* task);  // Uses configured algorithm
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

// Cancel a task (only PENDING tasks can be cancelled)
int cancel_task(TaskQueue* queue, int task_id);

// Scheduling algorithm functions
int set_scheduling_algorithm(TaskQueue* queue, SchedulingAlgorithm algorithm);
SchedulingAlgorithm get_scheduling_algorithm(TaskQueue* queue);

// MLFQ helper functions
void update_mlfq_priority(TaskQueue* queue, Task* task);

// Gang scheduling helper functions
int get_gang_size(TaskQueue* queue, int gang_id);
int dequeue_gang_tasks(TaskQueue* queue, int gang_id, Task* tasks, int max_tasks);

#endif // TASK_QUEUE_H

