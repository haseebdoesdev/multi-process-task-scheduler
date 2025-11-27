#!/bin/bash

# Monitor Script
# Continuously displays the current status of the task scheduler

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

# Check if scheduler is running
if [ ! -f scheduler.pid ]; then
    echo "Error: Scheduler is not running"
    exit 1
fi

PID=$(cat scheduler.pid)
if ! ps -p "$PID" > /dev/null 2>&1; then
    echo "Error: Scheduler is not running (stale PID file)"
    exit 1
fi

# Compile monitor helper if needed
if [ ! -f monitor_helper ]; then
    cat > monitor_helper.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "config.h"
#include "src/task_queue.h"
#include "src/common.h"

void print_task(Task* task) {
    char creation_time[64], start_time[64], end_time[64];
    format_timestamp(task->creation_time, creation_time, sizeof(creation_time));
    if (task->start_time > 0) {
        format_timestamp(task->start_time, start_time, sizeof(start_time));
    } else {
        strcpy(start_time, "N/A");
    }
    if (task->end_time > 0) {
        format_timestamp(task->end_time, end_time, sizeof(end_time));
    } else {
        strcpy(end_time, "N/A");
    }
    
    printf("  ID: %3d | %-20s | %-6s | %-8s | Worker: %2d | Retry: %d/%d | Timeout: %us\n",
           task->id, task->name, priority_to_string(task->priority),
           status_to_string(task->status), task->worker_id, 
           task->retry_count, MAX_TASK_RETRIES, task->timeout_seconds);
}

int main(void) {
    TaskQueue* queue = attach_shared_memory(-1);
    if (queue == NULL) {
        fprintf(stderr, "Error: Failed to attach to shared memory\n");
        return 1;
    }
    
    // Lock for safe reading
    pthread_mutex_lock(&queue->queue_mutex);
    
    // Print summary
    printf("\n=== Task Scheduler Status ===\n");
    printf("Total Tasks: %d | Completed: %d | Failed: %d\n",
           queue->total_tasks, queue->completed_tasks, queue->failed_tasks);
    printf("Active Workers: %d | Queue Size: %d/%d\n",
           queue->num_active_workers, queue->size, queue->capacity);
    printf("Pending: %d | Running: %d\n\n",
           get_pending_task_count(queue), get_running_task_count(queue));
    
    // Print tasks
    if (queue->size > 0) {
        printf("Tasks:\n");
        printf("  ID   | Name                 | Priority | Status   | Worker | Retry | Timeout\n");
        printf("  -----+----------------------+----------+----------+--------+-------+--------\n");
        
        for (int i = 0; i < queue->size; i++) {
            print_task(&queue->tasks[i]);
        }
    } else {
        printf("No tasks in queue.\n");
    }
    
    printf("\n");
    
    pthread_mutex_unlock(&queue->queue_mutex);
    detach_shared_memory(queue);
    return 0;
}
EOF
    gcc -o monitor_helper monitor_helper.c src/task_queue.c src/common.c -lpthread -I. || {
        echo "Error: Failed to compile monitor_helper"
        rm -f monitor_helper.c
        exit 1
    }
    rm -f monitor_helper.c
fi

# Clear screen and monitor
if command -v clear > /dev/null; then
    clear
fi

echo "Monitoring scheduler (PID: $PID). Press Ctrl+C to exit."
echo "Refreshing every ${MONITOR_REFRESH_INTERVAL:-2} seconds..."
echo ""

while true; do
    ./monitor_helper
    sleep ${MONITOR_REFRESH_INTERVAL:-2}
    
    # Clear screen for next update (if clear command exists)
    if command -v clear > /dev/null; then
        clear
        echo "Monitoring scheduler (PID: $PID). Press Ctrl+C to exit."
        echo ""
    else
        echo "---"
    fi
done

