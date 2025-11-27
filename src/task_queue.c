#include "task_queue.h"
#include "logger.h"
#include <sys/stat.h>

static int shm_id = -1;

int init_shared_memory(void) {
    size_t shm_size = sizeof(TaskQueue);
    int created = 0;
    
    // Create shared memory segment
    shm_id = shmget(SHM_KEY, shm_size, IPC_CREAT | IPC_EXCL | 0666);
    
    if (shm_id == -1) {
        if (errno == EEXIST) {
            // Already exists, try to get it
            shm_id = shmget(SHM_KEY, shm_size, 0666);
            if (shm_id == -1) {
                perror("shmget: Failed to access existing shared memory");
                return -1;
            }
            created = 0;
        } else {
            perror("shmget: Failed to create shared memory");
            return -1;
        }
    } else {
        created = 1;
    }
    
    // Attach and initialize
    TaskQueue* queue = (TaskQueue*)shmat(shm_id, NULL, 0);
    if (queue == (TaskQueue*)-1) {
        perror("shmat: Failed to attach shared memory");
        return -1;
    }
    
    // Initialize queue structure (only if we created it)
    if (created) {
        queue->size = 0;
        queue->capacity = MAX_TASKS;
        queue->next_task_id = 1;
        queue->total_tasks = 0;
        queue->completed_tasks = 0;
        queue->failed_tasks = 0;
        queue->num_active_workers = 0;
        queue->shutdown_flag = 0;
        queue->scheduler_pid = 0;
        
        // Initialize mutex with process-shared attribute
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&queue->queue_mutex, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);
        
        // Initialize condition variable
        pthread_condattr_t cond_attr;
        pthread_condattr_init(&cond_attr);
        pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&queue->queue_cond, &cond_attr);
        pthread_condattr_destroy(&cond_attr);
    }
    
    detach_shared_memory(queue);
    return shm_id;
}

TaskQueue* attach_shared_memory(int shm_id_to_attach) {
    if (shm_id_to_attach == -1) {
        shm_id_to_attach = shmget(SHM_KEY, sizeof(TaskQueue), 0666);
        if (shm_id_to_attach == -1) {
            perror("shmget: Failed to attach to shared memory");
            return NULL;
        }
    }
    
    TaskQueue* queue = (TaskQueue*)shmat(shm_id_to_attach, NULL, 0);
    if (queue == (TaskQueue*)-1) {
        perror("shmat: Failed to attach shared memory");
        return NULL;
    }
    
    shm_id = shm_id_to_attach;
    return queue;
}

void detach_shared_memory(TaskQueue* queue) {
    if (queue != NULL && queue != (TaskQueue*)-1) {
        shmdt(queue);
    }
}

void destroy_shared_memory(int shm_id_to_destroy) {
    if (shm_id_to_destroy != -1) {
        shmctl(shm_id_to_destroy, IPC_RMID, NULL);
    }
}

int enqueue_task(TaskQueue* queue, const char* name, Priority priority, unsigned int execution_time_ms, unsigned int timeout_seconds) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    if (is_queue_full(queue)) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return -1;
    }
    
    // Use binary search to find insertion point (O(log n) instead of O(n))
    // We're inserting into a priority-sorted array
    int left = 0;
    int right = queue->size;
    int insert_pos = queue->size;
    
    while (left < right) {
        int mid = left + (right - left) / 2;
        // Lower priority number = higher priority
        // We want to insert before tasks with lower priority (higher number)
        if (queue->tasks[mid].priority > priority) {
            insert_pos = mid;
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    
    // Shift tasks to make room (still O(n) but necessary for array structure)
    for (int i = queue->size; i > insert_pos; i--) {
        queue->tasks[i] = queue->tasks[i - 1];
    }
    
    // Insert new task
    Task* task = &queue->tasks[insert_pos];
    task->id = queue->next_task_id++;
    strncpy(task->name, name, MAX_TASK_NAME_LEN - 1);
    task->name[MAX_TASK_NAME_LEN - 1] = '\0';
    task->priority = priority;
    task->status = STATUS_PENDING;
    task->creation_time = time(NULL);
    task->start_time = 0;
    task->end_time = 0;
    task->execution_time_ms = execution_time_ms;
    task->timeout_seconds = timeout_seconds;  // 0 = no timeout
    task->retry_count = 0;
    task->worker_id = -1;
    task->thread_id = 0;
    
    queue->size++;
    queue->total_tasks++;
    
    pthread_cond_signal(&queue->queue_cond);
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return task->id;
}

int dequeue_task(TaskQueue* queue, Task* task) {
    if (queue == NULL || task == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    // Since array is sorted by priority, first pending task is highest priority
    // This makes dequeue O(n) worst case, but O(1) best case (first task is pending)
    int found_idx = -1;
    for (int i = 0; i < queue->size; i++) {
        if (queue->tasks[i].status == STATUS_PENDING) {
            found_idx = i;
            break;
        }
    }
    
    if (found_idx == -1) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return -1;
    }
    
    // Copy task
    *task = queue->tasks[found_idx];
    task->status = STATUS_RUNNING;
    task->start_time = time(NULL);
    
    // Update in queue
    queue->tasks[found_idx].status = STATUS_RUNNING;
    queue->tasks[found_idx].start_time = task->start_time;
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return task->id;
}

int update_task_status(TaskQueue* queue, int task_id, TaskStatus new_status, time_t* time_field) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    Task* task = find_task_by_id(queue, task_id);
    if (task == NULL) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return -1;
    }
    
    // Get old status before updating
    TaskStatus old_status = task->status;
    
    task->status = new_status;
    if (time_field != NULL) {
        *time_field = time(NULL);
        if (new_status == STATUS_COMPLETED || new_status == STATUS_FAILED || new_status == STATUS_TIMEOUT) {
            task->end_time = *time_field;
        }
    }
    
    // Only increment counters if status actually changed from non-terminal to terminal
    // This prevents double-counting
    if (old_status != new_status) {
        if (new_status == STATUS_COMPLETED && old_status != STATUS_COMPLETED) {
            queue->completed_tasks++;
        } else if ((new_status == STATUS_FAILED || new_status == STATUS_TIMEOUT) && 
                   old_status != STATUS_FAILED && old_status != STATUS_TIMEOUT) {
            queue->failed_tasks++;
        }
        // Decrement counters if changing FROM completed/failed/timeout to something else (shouldn't happen, but safe)
        if (old_status == STATUS_COMPLETED && new_status != STATUS_COMPLETED) {
            queue->completed_tasks--;
        } else if ((old_status == STATUS_FAILED || old_status == STATUS_TIMEOUT) && 
                   new_status != STATUS_FAILED && new_status != STATUS_TIMEOUT) {
            queue->failed_tasks--;
        }
    }
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return 0;
}

Task* find_task_by_id(TaskQueue* queue, int task_id) {
    if (queue == NULL) return NULL;
    
    for (int i = 0; i < queue->size; i++) {
        if (queue->tasks[i].id == task_id) {
            return &queue->tasks[i];
        }
    }
    
    return NULL;
}

int is_queue_full(TaskQueue* queue) {
    if (queue == NULL) return 1;
    return queue->size >= queue->capacity;
}

int is_queue_empty(TaskQueue* queue) {
    if (queue == NULL) return 1;
    return queue->size == 0;
}

int get_pending_task_count(TaskQueue* queue) {
    if (queue == NULL) return 0;
    
    // This function must be called with mutex already locked
    // If called from outside, caller must lock
    
    int count = 0;
    for (int i = 0; i < queue->size; i++) {
        if (queue->tasks[i].status == STATUS_PENDING) {
            count++;
        }
    }
    return count;
}

int get_running_task_count(TaskQueue* queue) {
    if (queue == NULL) return 0;
    
    // This function must be called with mutex already locked
    // If called from outside, caller must lock
    
    int count = 0;
    for (int i = 0; i < queue->size; i++) {
        if (queue->tasks[i].status == STATUS_RUNNING) {
            count++;
        }
    }
    return count;
}

// Thread-safe versions that lock internally
int get_pending_task_count_safe(TaskQueue* queue) {
    if (queue == NULL) return 0;
    
    pthread_mutex_lock(&queue->queue_mutex);
    int count = get_pending_task_count(queue);
    pthread_mutex_unlock(&queue->queue_mutex);
    return count;
}

int get_running_task_count_safe(TaskQueue* queue) {
    if (queue == NULL) return 0;
    
    pthread_mutex_lock(&queue->queue_mutex);
    int count = get_running_task_count(queue);
    pthread_mutex_unlock(&queue->queue_mutex);
    return count;
}

int remove_completed_tasks(TaskQueue* queue, int max_age_seconds) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    time_t current_time = time(NULL);
    int removed = 0;
    int write_idx = 0;
    
    // Compact array by removing old completed/failed/timeout tasks
    for (int read_idx = 0; read_idx < queue->size; read_idx++) {
        Task* task = &queue->tasks[read_idx];
        
        // Keep task if:
        // 1. Not in terminal state (COMPLETED/FAILED/TIMEOUT), OR
        // 2. In terminal state but not old enough
        int should_keep = 1;
        if (task->status == STATUS_COMPLETED || task->status == STATUS_FAILED || task->status == STATUS_TIMEOUT) {
            if (task->end_time > 0) {
                int age = (int)difftime(current_time, task->end_time);
                if (age > max_age_seconds) {
                    should_keep = 0;
                    removed++;
                }
            }
        }
        
        if (should_keep) {
            if (write_idx != read_idx) {
                queue->tasks[write_idx] = queue->tasks[read_idx];
            }
            write_idx++;
        }
    }
    
    queue->size = write_idx;
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return removed;
}

// Recover orphaned tasks from a crashed worker
int recover_orphaned_tasks(TaskQueue* queue, int dead_worker_id) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    int recovered = 0;
    time_t current_time = time(NULL);
    
    for (int i = 0; i < queue->size; i++) {
        Task* task = &queue->tasks[i];
        
        // Find tasks that were running on the dead worker
        if (task->status == STATUS_RUNNING && task->worker_id == dead_worker_id) {
            // Check if we should retry
            if (task->retry_count < MAX_TASK_RETRIES) {
                // Reset task to pending for retry
                task->status = STATUS_PENDING;
                task->retry_count++;
                task->worker_id = -1;
                task->start_time = 0;
                task->thread_id = 0;
                recovered++;
                
                LOG_INFO_F("Recovered task %d (retry %d/%d) from crashed worker %d", 
                          task->id, task->retry_count, MAX_TASK_RETRIES, dead_worker_id);
            } else {
                // Max retries exceeded, mark as failed
                task->status = STATUS_FAILED;
                task->end_time = current_time;
                queue->failed_tasks++;
                
                LOG_WARN_F("Task %d exceeded max retries (%d), marking as FAILED", 
                          task->id, MAX_TASK_RETRIES);
            }
        }
    }
    
    // Signal workers that new tasks are available
    if (recovered > 0) {
        pthread_cond_broadcast(&queue->queue_cond);
    }
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return recovered;
}

// Check and handle timed-out tasks
int check_and_handle_timeouts(TaskQueue* queue) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    int timed_out = 0;
    time_t current_time = time(NULL);
    
    for (int i = 0; i < queue->size; i++) {
        Task* task = &queue->tasks[i];
        
        // Check running tasks for timeout
        if (task->status == STATUS_RUNNING && task->timeout_seconds > 0 && task->start_time > 0) {
            int elapsed = (int)difftime(current_time, task->start_time);
            
            if (elapsed >= (int)task->timeout_seconds) {
                // Task has timed out
                if (task->retry_count < MAX_TASK_RETRIES) {
                    // Reset for retry
                    task->status = STATUS_PENDING;
                    task->retry_count++;
                    task->worker_id = -1;
                    task->start_time = 0;
                    task->thread_id = 0;
                    timed_out++;
                    
                    LOG_WARN_F("Task %d timed out after %d seconds (retry %d/%d)", 
                              task->id, elapsed, task->retry_count, MAX_TASK_RETRIES);
                } else {
                    // Max retries exceeded, mark as timeout
                    task->status = STATUS_TIMEOUT;
                    task->end_time = current_time;
                    queue->failed_tasks++;
                    timed_out++;
                    
                    LOG_ERROR_F("Task %d timed out and exceeded max retries, marking as TIMEOUT", 
                               task->id);
                }
            }
        }
    }
    
    // Signal workers that new tasks are available (if any were reset to pending)
    if (timed_out > 0) {
        pthread_cond_broadcast(&queue->queue_cond);
    }
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return timed_out;
}

