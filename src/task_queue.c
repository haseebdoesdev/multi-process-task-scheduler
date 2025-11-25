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

int enqueue_task(TaskQueue* queue, const char* name, Priority priority, unsigned int execution_time_ms) {
    if (queue == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    if (is_queue_full(queue)) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return -1;
    }
    
    // Find insertion point based on priority (lower number = higher priority)
    int insert_pos = queue->size;
    for (int i = 0; i < queue->size; i++) {
        if (queue->tasks[i].priority > priority) {
            insert_pos = i;
            break;
        }
    }
    
    // Shift tasks to make room
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
    
    queue->size++;
    queue->total_tasks++;
    
    pthread_cond_signal(&queue->queue_cond);
    pthread_mutex_unlock(&queue->queue_mutex);
    
    return task->id;
}

int dequeue_task(TaskQueue* queue, Task* task) {
    if (queue == NULL || task == NULL) return -1;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    // Find highest priority pending task
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
    
    task->status = new_status;
    if (time_field != NULL) {
        *time_field = time(NULL);
        if (new_status == STATUS_COMPLETED || new_status == STATUS_FAILED) {
            task->end_time = *time_field;
        }
    }
    
    if (new_status == STATUS_COMPLETED) {
        queue->completed_tasks++;
    } else if (new_status == STATUS_FAILED) {
        queue->failed_tasks++;
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
    
    int count = 0;
    for (int i = 0; i < queue->size; i++) {
        if (queue->tasks[i].status == STATUS_RUNNING) {
            count++;
        }
    }
    return count;
}

