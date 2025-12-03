#include "task_queue.h"
#include "logger.h"
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

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
        queue->algorithm = SCHED_ALGORITHM_PRIORITY;  // Default algorithm
        queue->mlfq_time_slice_ms = 1000;  // 1 second time slice
        queue->mlfq_max_cpu_time_ms = 5000;  // 5 seconds max before demotion
        queue->rr_last_index = -1;  // Round Robin: start from beginning
        queue->rr_time_quantum_ms = 2000;  // 2 second time quantum for Round Robin
        queue->num_cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);  // Detect CPU cores
        if (queue->num_cpu_cores <= 0) queue->num_cpu_cores = 1;  // Fallback
        
        // Seed random number generator for lottery scheduling (only once)
        srand((unsigned int)time(NULL));
        
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

int enqueue_task(TaskQueue* queue, const char* name, Priority priority, unsigned int execution_time_ms) {
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
    task->worker_id = -1;
    task->thread_id = 0;
    task->deadline_time = 0;
    task->gang_id = -1;
    task->cpu_time_used = 0;
    task->current_mlfq_level = priority;
    task->mlfq_level_start = time(NULL);
    task->lottery_tickets = 10;  // Default tickets for lottery scheduling
    task->remaining_time_ms = execution_time_ms;  // For SRTF
    
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
        if (new_status == STATUS_COMPLETED || new_status == STATUS_FAILED) {
            task->end_time = *time_field;
        }
    }
    
    // Only increment counters if status actually changed from non-terminal to terminal
    // This prevents double-counting
    if (old_status != new_status) {
        if (new_status == STATUS_COMPLETED && old_status != STATUS_COMPLETED) {
            queue->completed_tasks++;
        } else if (new_status == STATUS_FAILED && old_status != STATUS_FAILED) {
            queue->failed_tasks++;
        }
        // Decrement counters if changing FROM completed/failed to something else (shouldn't happen, but safe)
        if (old_status == STATUS_COMPLETED && new_status != STATUS_COMPLETED) {
            queue->completed_tasks--;
        } else if (old_status == STATUS_FAILED && new_status != STATUS_FAILED) {
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
    
    // Compact array by removing old completed/failed tasks
    for (int read_idx = 0; read_idx < queue->size; read_idx++) {
        Task* task = &queue->tasks[read_idx];
        
        // Keep task if:
        // 1. Not in terminal state (COMPLETED/FAILED), OR
        // 2. In terminal state but not old enough
        int should_keep = 1;
        if (task->status == STATUS_COMPLETED || task->status == STATUS_FAILED) {
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

int cancel_task(TaskQueue* queue, int task_id) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    Task* task = find_task_by_id(queue, task_id);
    if (task == NULL) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return -1; // Task not found
    }
    
    // Only PENDING tasks can be cancelled
    if (task->status != STATUS_PENDING) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return -2; // Task not in cancellable state
    }
    
    // Mark as failed (cancelled)
    task->status = STATUS_FAILED;
    task->end_time = time(NULL);
    queue->failed_tasks++;
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return 0; // Success
}

// Advanced enqueue with deadline and gang support
int enqueue_task_advanced(TaskQueue* queue, const char* name, Priority priority, unsigned int execution_time_ms, 
                          time_t deadline_time, int gang_id) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    if (is_queue_full(queue)) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return -1;
    }
    
    // Find insertion point based on algorithm
    int insert_pos = queue->size;
    
    if (queue->algorithm == SCHED_ALGORITHM_EDF && deadline_time > 0) {
        // EDF: Sort by deadline (earliest first)
        for (int i = 0; i < queue->size; i++) {
            if (queue->tasks[i].status == STATUS_PENDING) {
                // If task has deadline and new task has deadline, compare
                if (queue->tasks[i].deadline_time > 0 && deadline_time > 0) {
                    if (deadline_time < queue->tasks[i].deadline_time) {
                        insert_pos = i;
                        break;
                    }
                } else if (deadline_time > 0) {
                    // New task has deadline, existing doesn't - prioritize deadline
                    insert_pos = i;
                    break;
                }
            }
        }
    } else {
        // Priority-based: Use binary search as before
        int left = 0;
        int right = queue->size;
        
        while (left < right) {
            int mid = left + (right - left) / 2;
            if (queue->tasks[mid].priority > priority) {
                insert_pos = mid;
                right = mid;
            } else {
                left = mid + 1;
            }
        }
    }
    
    // Shift tasks
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
    task->worker_id = -1;
    task->thread_id = 0;
    task->deadline_time = deadline_time;
    task->gang_id = gang_id;
    task->cpu_time_used = 0;
    task->current_mlfq_level = priority;
    task->mlfq_level_start = time(NULL);
    task->lottery_tickets = 10;  // Default tickets for lottery scheduling
    task->remaining_time_ms = execution_time_ms;  // For SRTF
    
    queue->size++;
    queue->total_tasks++;
    
    pthread_cond_signal(&queue->queue_cond);
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return task->id;
}

// Dequeue using configured algorithm
int dequeue_task_algorithm(TaskQueue* queue, Task* task) {
    if (queue == NULL || task == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    int found_idx = -1;
    time_t current_time = time(NULL);
    
    if (queue->algorithm == SCHED_ALGORITHM_EDF) {
        // EDF: Find task with earliest deadline
        time_t earliest_deadline = 0;
        for (int i = 0; i < queue->size; i++) {
            if (queue->tasks[i].status == STATUS_PENDING) {
                if (queue->tasks[i].deadline_time > 0) {
                    if (found_idx == -1 || queue->tasks[i].deadline_time < earliest_deadline) {
                        found_idx = i;
                        earliest_deadline = queue->tasks[i].deadline_time;
                    }
                } else if (found_idx == -1) {
                    // Task without deadline - use as fallback
                    found_idx = i;
                }
            }
        }
    } else if (queue->algorithm == SCHED_ALGORITHM_GANG) {
        // Gang: Find first task in a gang, then get all gang members
        // For simplicity, return first pending task (gang coordination in worker)
        for (int i = 0; i < queue->size; i++) {
            if (queue->tasks[i].status == STATUS_PENDING) {
                found_idx = i;
                break;
            }
        }
    } else if (queue->algorithm == SCHED_ALGORITHM_ROUND_ROBIN) {
        // Round Robin: Circular scheduling from last index
        int start_idx = (queue->rr_last_index + 1) % queue->size;
        for (int i = 0; i < queue->size; i++) {
            int idx = (start_idx + i) % queue->size;
            if (queue->tasks[idx].status == STATUS_PENDING) {
                found_idx = idx;
                break;
            }
        }
        if (found_idx >= 0) {
            queue->rr_last_index = found_idx;
        }
    } else if (queue->algorithm == SCHED_ALGORITHM_FIFO) {
        // FIFO/FCFS: First come, first served (oldest creation time first)
        time_t oldest_time = 0;
        for (int i = 0; i < queue->size; i++) {
            if (queue->tasks[i].status == STATUS_PENDING) {
                if (found_idx == -1 || queue->tasks[i].creation_time < oldest_time) {
                    found_idx = i;
                    oldest_time = queue->tasks[i].creation_time;
                }
            }
        }
    } else if (queue->algorithm == SCHED_ALGORITHM_SJF) {
        // Shortest Job First: Schedule task with smallest execution time
        unsigned int shortest_time = UINT_MAX;
        for (int i = 0; i < queue->size; i++) {
            if (queue->tasks[i].status == STATUS_PENDING) {
                if (queue->tasks[i].execution_time_ms < shortest_time) {
                    found_idx = i;
                    shortest_time = queue->tasks[i].execution_time_ms;
                }
            }
        }
    } else if (queue->algorithm == SCHED_ALGORITHM_SRTF) {
        // Shortest Remaining Time First: Schedule task with smallest remaining time
        unsigned int shortest_remaining = UINT_MAX;
        for (int i = 0; i < queue->size; i++) {
            if (queue->tasks[i].status == STATUS_PENDING) {
                // Update remaining time if task was previously running but preempted
                if (queue->tasks[i].remaining_time_ms == 0) {
                    queue->tasks[i].remaining_time_ms = queue->tasks[i].execution_time_ms;
                }
                if (queue->tasks[i].remaining_time_ms < shortest_remaining) {
                    found_idx = i;
                    shortest_remaining = queue->tasks[i].remaining_time_ms;
                }
            }
        }
    } else if (queue->algorithm == SCHED_ALGORITHM_LOTTERY) {
        // Lottery Scheduling: Weighted random selection based on tickets
        unsigned int total_tickets = 0;
        for (int i = 0; i < queue->size; i++) {
            if (queue->tasks[i].status == STATUS_PENDING) {
                total_tickets += queue->tasks[i].lottery_tickets;
            }
        }
        
        if (total_tickets > 0) {
            // Generate random number between 0 and total_tickets
            unsigned int winning_ticket = (unsigned int)(rand() % total_tickets);
            unsigned int ticket_count = 0;
            
            for (int i = 0; i < queue->size; i++) {
                if (queue->tasks[i].status == STATUS_PENDING) {
                    ticket_count += queue->tasks[i].lottery_tickets;
                    if (ticket_count > winning_ticket) {
                        found_idx = i;
                        break;
                    }
                }
            }
        }
    } else {
        // Priority or MLFQ: Find highest priority pending task
        Priority best_priority = PRIORITY_LOW + 1;  // Start higher than any valid priority
        for (int i = 0; i < queue->size; i++) {
            if (queue->tasks[i].status == STATUS_PENDING) {
                Priority task_priority = (queue->algorithm == SCHED_ALGORITHM_MLFQ) 
                    ? queue->tasks[i].current_mlfq_level 
                    : queue->tasks[i].priority;
                
                if (task_priority < best_priority) {
                    found_idx = i;
                    best_priority = task_priority;
                }
            }
        }
    }
    
    if (found_idx == -1) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return -1;
    }
    
    // Copy task
    *task = queue->tasks[found_idx];
    task->status = STATUS_RUNNING;
    task->start_time = current_time;
    
    // Update in queue
    queue->tasks[found_idx].status = STATUS_RUNNING;
    queue->tasks[found_idx].start_time = task->start_time;
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return task->id;
}

// Set scheduling algorithm
int set_scheduling_algorithm(TaskQueue* queue, SchedulingAlgorithm algorithm) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    queue->algorithm = algorithm;
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return 0;
}

// Get scheduling algorithm
SchedulingAlgorithm get_scheduling_algorithm(TaskQueue* queue) {
    if (queue == NULL) return SCHED_ALGORITHM_PRIORITY;
    
    pthread_mutex_lock(&queue->queue_mutex);
    SchedulingAlgorithm alg = queue->algorithm;
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return alg;
}

// Update MLFQ priority based on CPU time used
void update_mlfq_priority(TaskQueue* queue, Task* task) {
    if (queue == NULL || task == NULL || queue->algorithm != SCHED_ALGORITHM_MLFQ) {
        return;
    }
    
    // Check if task exceeded time slice for current level
    time_t current_time = time(NULL);
    unsigned int time_in_level = (unsigned int)difftime(current_time, task->mlfq_level_start) * 1000;
    
    // If exceeded time slice and not already at lowest priority, demote
    if (time_in_level > queue->mlfq_time_slice_ms && task->current_mlfq_level < PRIORITY_LOW) {
        task->current_mlfq_level = (Priority)(task->current_mlfq_level + 1);
        task->mlfq_level_start = current_time;
        task->cpu_time_used += time_in_level;
    }
}

// Get number of tasks in a gang
int get_gang_size(TaskQueue* queue, int gang_id) {
    if (queue == NULL || gang_id < 0) return 0;
    
    int count = 0;
    for (int i = 0; i < queue->size; i++) {
        if (queue->tasks[i].gang_id == gang_id && queue->tasks[i].status == STATUS_PENDING) {
            count++;
        }
    }
    return count;
}

// Dequeue all tasks in a gang
int dequeue_gang_tasks(TaskQueue* queue, int gang_id, Task* tasks, int max_tasks) {
    if (queue == NULL || tasks == NULL || gang_id < 0) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    int count = 0;
    time_t current_time = time(NULL);
    
    for (int i = 0; i < queue->size && count < max_tasks; i++) {
        if (queue->tasks[i].gang_id == gang_id && queue->tasks[i].status == STATUS_PENDING) {
            tasks[count] = queue->tasks[i];
            tasks[count].status = STATUS_RUNNING;
            tasks[count].start_time = current_time;
            
            queue->tasks[i].status = STATUS_RUNNING;
            queue->tasks[i].start_time = current_time;
            
            count++;
        }
    }
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return count;
}

