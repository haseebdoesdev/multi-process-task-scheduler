#define _GNU_SOURCE
#include "common.h"
#include "task_queue.h"
#include "logger.h"
#include <sys/wait.h>
#include <sched.h>

static TaskQueue* queue = NULL;
static int worker_id = -1;
static volatile int shutdown_requested = 0;

// Thread data structure
typedef struct {
    Task task;
    TaskQueue* queue;
    int worker_id;
} ThreadData;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        shutdown_requested = 1;
        LOG_INFO_F("Worker %d: Shutdown signal received", worker_id);
        
        if (queue != NULL) {
            queue->shutdown_flag = 1;
            pthread_cond_broadcast(&queue->queue_cond);
        }
    }
}

void* task_executor_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    Task task = data->task;
    TaskQueue* q = data->queue;
    int wid = data->worker_id;
    
    // Update MLFQ priority tracking during execution
    if (q->algorithm == SCHED_ALGORITHM_MLFQ) {
        pthread_mutex_lock(&q->queue_mutex);
        Task* task_ptr = find_task_by_id(q, task.id);
        if (task_ptr) {
            update_mlfq_priority(q, task_ptr);
        }
        pthread_mutex_unlock(&q->queue_mutex);
    }
    
    LOG_INFO_F("Worker %d: Thread executing task %d: %s (priority: %s, duration: %u ms)",
               wid, task.id, task.name, priority_to_string(task.priority), task.execution_time_ms);
    
    // Simulate task execution by sleeping
    // For MLFQ, track CPU time usage
    // For SRTF, track remaining time
    unsigned int remaining_time = task.execution_time_ms;
    if (q->algorithm == SCHED_ALGORITHM_SRTF && task.remaining_time_ms > 0) {
        remaining_time = task.remaining_time_ms;  // Use remaining time for SRTF
    }
    
    while (remaining_time > 0 && !shutdown_requested) {
        unsigned int chunk = (remaining_time > 100) ? 100 : remaining_time;  // Check every 100ms
        usleep(chunk * 1000);
        remaining_time -= chunk;
        
        // Update task state in shared memory
        pthread_mutex_lock(&q->queue_mutex);
        Task* task_ptr = find_task_by_id(q, task.id);
        if (task_ptr && task_ptr->status == STATUS_RUNNING) {
            // Update CPU time used for MLFQ
            if (q->algorithm == SCHED_ALGORITHM_MLFQ) {
                task_ptr->cpu_time_used += chunk;
                update_mlfq_priority(q, task_ptr);
            }
            // Update remaining time for SRTF
            if (q->algorithm == SCHED_ALGORITHM_SRTF) {
                if (task_ptr->remaining_time_ms > chunk) {
                    task_ptr->remaining_time_ms -= chunk;
                } else {
                    task_ptr->remaining_time_ms = 0;
                }
            }
        }
        pthread_mutex_unlock(&q->queue_mutex);
    }
    
    // Update task status to completed
    time_t end_time;
    if (update_task_status(q, task.id, STATUS_COMPLETED, &end_time) == 0) {
        LOG_INFO_F("Worker %d: Task %d completed successfully", wid, task.id);
    } else {
        LOG_ERROR_F("Worker %d: Failed to update status for task %d", wid, task.id);
        update_task_status(q, task.id, STATUS_FAILED, NULL);
    }
    
    free(data);
    return NULL;
}

int execute_task(TaskQueue* queue, Task* task) {
    if (task == NULL || queue == NULL) return -1;
    
    // Worker ID and status already set in worker_main_loop
    // No need to lock here again
    
    // Create thread data
    ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
    if (data == NULL) {
        LOG_ERROR_F("Worker %d: Failed to allocate thread data", worker_id);
        update_task_status(queue, task->id, STATUS_FAILED, NULL);
        return -1;
    }
    
    data->task = *task;
    data->queue = queue;
    data->worker_id = worker_id;
    
    // Create thread to execute task
    pthread_t thread;
    if (pthread_create(&thread, NULL, task_executor_thread, data) != 0) {
        LOG_ERROR_F("Worker %d: Failed to create thread for task %d", worker_id, task->id);
        update_task_status(queue, task->id, STATUS_FAILED, NULL);
        free(data);
        return -1;
    }
    
    // Detach thread (we don't need to join)
    pthread_detach(thread);
    
    return 0;
}

void worker_main_loop(void) {
    LOG_INFO_F("Worker %d: Starting main loop", worker_id);
    
    while (!shutdown_requested && !(queue && queue->shutdown_flag)) {
        Task task;
        
        // Use condition variable to wait for tasks instead of polling
        pthread_mutex_lock(&queue->queue_mutex);
        
        // Wait for tasks to become available or shutdown
        while ((is_queue_empty(queue) || get_pending_task_count(queue) == 0) 
               && !shutdown_requested 
               && !queue->shutdown_flag) {
            pthread_cond_wait(&queue->queue_cond, &queue->queue_mutex);
        }
        
        // Check if we should exit
        if (shutdown_requested || queue->shutdown_flag) {
            pthread_mutex_unlock(&queue->queue_mutex);
            break;
        }
        
        // Try to dequeue a task using the configured algorithm
        // Note: dequeue_task_algorithm locks/unlocks mutex internally,
        // so we need to unlock first
        pthread_mutex_unlock(&queue->queue_mutex);
        
        // Use algorithm-aware dequeue
        if (dequeue_task_algorithm(queue, &task) < 0) {
            continue;  // No task available
        }
        
        // Set worker ID (need to lock again to update)
        pthread_mutex_lock(&queue->queue_mutex);
        Task* found_task = find_task_by_id(queue, task.id);
        if (found_task) {
            found_task->worker_id = worker_id;
            task.worker_id = worker_id;
        }
        pthread_mutex_unlock(&queue->queue_mutex);
        
        // Execute the task (outside of lock)
        execute_task(queue, &task);
    }
    
    LOG_INFO_F("Worker %d: Main loop exiting", worker_id);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <worker_id>\n", argv[0]);
        return 1;
    }
    
    worker_id = atoi(argv[1]);
    
    // Initialize logger
    char log_name[64];
    snprintf(log_name, sizeof(log_name), "worker_%d", worker_id);
    init_logger(log_name);
    
    LOG_INFO_F("Worker %d starting (PID: %d)", worker_id, getpid());
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Attach to shared memory
    queue = attach_shared_memory(-1);
    if (queue == NULL) {
        LOG_ERROR_F("Worker %d: Failed to attach to shared memory", worker_id);
        return 1;
    }
    
    LOG_INFO_F("Worker %d: Attached to shared memory", worker_id);
    
    // Set CPU affinity: Pin worker to a specific CPU core
    if (queue->num_cpu_cores > 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        int core_id = worker_id % queue->num_cpu_cores;
        CPU_SET(core_id, &cpuset);
        
        if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0) {
            LOG_INFO_F("Worker %d: Pinned to CPU core %d (total cores: %d)", worker_id, core_id, queue->num_cpu_cores);
        } else {
            LOG_ERROR_F("Worker %d: Failed to set CPU affinity: %s", worker_id, strerror(errno));
        }
    }
    
    // Register worker as active
    pthread_mutex_lock(&queue->queue_mutex);
    queue->num_active_workers++;
    pthread_mutex_unlock(&queue->queue_mutex);
    
    // Main worker loop
    worker_main_loop();
    
    // Unregister worker
    pthread_mutex_lock(&queue->queue_mutex);
    if (queue->num_active_workers > 0) {
        queue->num_active_workers--;
    }
    pthread_mutex_unlock(&queue->queue_mutex);
    
    // Detach from shared memory
    detach_shared_memory(queue);
    
    LOG_INFO_F("Worker %d: Shutting down", worker_id);
    close_logger();
    
    return 0;
}

