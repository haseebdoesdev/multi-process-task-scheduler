#include "common.h"
#include "task_queue.h"
#include "logger.h"
#include <sys/wait.h>

static TaskQueue* queue = NULL;
static int shm_id = -1;
static pid_t worker_pids[NUM_WORKERS];
static int num_workers_running = 0;
static volatile int shutdown_requested = 0;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        shutdown_requested = 1;
        LOG_INFO_F("Shutdown signal received");
        
        if (queue != NULL) {
            queue->shutdown_flag = 1;
            pthread_cond_broadcast(&queue->queue_cond);
        }
    }
}

void cleanup_resources(void) {
    LOG_INFO_F("Cleaning up resources...");
    
    // Wait for workers to finish
    for (int i = 0; i < num_workers_running; i++) {
        if (worker_pids[i] > 0) {
            LOG_INFO_F("Waiting for worker %d (PID: %d)", i, worker_pids[i]);
            kill(worker_pids[i], SIGTERM);
            waitpid(worker_pids[i], NULL, 0);
        }
    }
    
    // Set shutdown flag
    if (queue != NULL) {
        queue->shutdown_flag = 1;
        pthread_cond_broadcast(&queue->queue_cond);
    }
    
    // Detach shared memory
    if (queue != NULL) {
        detach_shared_memory(queue);
        queue = NULL;
    }
    
    // Destroy shared memory (optional - might want to keep it for monitoring)
    // destroy_shared_memory(shm_id);
    
    close_logger();
}

int spawn_worker(int worker_id) {
    pid_t pid = fork();
    
    if (pid < 0) {
        LOG_ERROR_F("Failed to fork worker %d: %s", worker_id, strerror(errno));
        return -1;
    } else if (pid == 0) {
        // Child process - exec worker
        char worker_id_str[16];
        snprintf(worker_id_str, sizeof(worker_id_str), "%d", worker_id);
        
        execl("./worker", "worker", worker_id_str, NULL);
        // If execl fails
        LOG_ERROR_F("Failed to exec worker: %s", strerror(errno));
        exit(1);
    } else {
        // Parent process
        worker_pids[worker_id] = pid;
        LOG_INFO_F("Spawned worker %d with PID %d", worker_id, pid);
        return 0;
    }
}

void monitor_workers(void) {
    while (!shutdown_requested) {
        sleep(WORKER_CHECK_INTERVAL);
        
        // Check if any workers have died
        for (int i = 0; i < num_workers_running; i++) {
            if (worker_pids[i] > 0) {
                int status;
                pid_t result = waitpid(worker_pids[i], &status, WNOHANG);
                
                if (result > 0) {
                    LOG_WARN_F("Worker %d (PID: %d) exited with status %d", 
                              i, worker_pids[i], WEXITSTATUS(status));
                    // Try to respawn
                    worker_pids[i] = 0;
                    if (spawn_worker(i) == 0) {
                        LOG_INFO_F("Respawned worker %d", i);
                    }
                }
            }
        }
        
        // Update worker count in shared memory
        if (queue != NULL) {
            int active = 0;
            for (int i = 0; i < num_workers_running; i++) {
                if (worker_pids[i] > 0 && kill(worker_pids[i], 0) == 0) {
                    active++;
                }
            }
            queue->num_active_workers = active;
        }
    }
}

int main(int argc, char* argv[]) {
    // Initialize logger
    init_logger("scheduler");
    LOG_INFO_F("Starting scheduler...");
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGCHLD, SIG_IGN); // Reap zombie processes
    
    // Register cleanup function
    atexit(cleanup_resources);
    
    // Initialize shared memory
    shm_id = init_shared_memory();
    if (shm_id == -1) {
        LOG_ERROR_F("Failed to initialize shared memory");
        return 1;
    }
    
    // Attach to shared memory
    queue = attach_shared_memory(shm_id);
    if (queue == NULL) {
        LOG_ERROR_F("Failed to attach to shared memory");
        return 1;
    }
    
    // Set scheduler PID
    queue->scheduler_pid = getpid();
    
    // Write PID to file
    FILE* pid_file = fopen(PID_FILE, "w");
    if (pid_file) {
        fprintf(pid_file, "%d\n", getpid());
        fclose(pid_file);
    }
    
    LOG_INFO_F("Shared memory initialized, scheduler PID: %d", getpid());
    
    // Spawn worker processes
    num_workers_running = NUM_WORKERS;
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (spawn_worker(i) != 0) {
            LOG_ERROR_F("Failed to spawn worker %d", i);
            num_workers_running--;
        }
    }
    
    LOG_INFO_F("Started %d worker processes", num_workers_running);
    
    // Main scheduler loop - monitor workers
    monitor_workers();
    
    LOG_INFO_F("Scheduler shutting down...");
    return 0;
}

