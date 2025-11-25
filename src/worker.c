#include "common.h"
#include "task_queue.h"
#include "logger.h"
#include <sys/wait.h>

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
    
    LOG_INFO_F("Worker %d: Thread executing task %d: %s (priority: %s, duration: %u ms)",
               wid, task.id, task.name, priority_to_string(task.priority), task.execution_time_ms);
    
    // Simulate task execution by sleeping
    usleep(task.execution_time_ms * 1000); // Convert ms to microseconds
    
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
    
    // Update task status to running and set worker ID
    pthread_mutex_lock(&queue->queue_mutex);
    Task* queue_task = find_task_by_id(queue, task->id);
    if (queue_task != NULL) {
        queue_task->status = STATUS_RUNNING;
        queue_task->start_time = time(NULL);
        queue_task->worker_id = worker_id;
    }
    pthread_mutex_unlock(&queue->queue_mutex);
    
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
        
        // Try to dequeue a task
        if (dequeue_task(queue, &task) == -1) {
            // No tasks available, wait a bit
            usleep(100000); // 100ms
            continue;
        }
        
        // Execute the task
        execute_task(queue, &task);
        
        // Small delay to prevent CPU spinning
        usleep(10000); // 10ms
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

