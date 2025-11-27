#!/bin/bash

# Report Script
# Generates a CSV report of all tasks

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

OUTPUT_FILE="${1:-report_$(date +%Y%m%d_%H%M%S).csv}"

# Compile report helper if needed
if [ ! -f report_helper ]; then
    cat > report_helper.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "config.h"
#include "src/task_queue.h"
#include "src/common.h"

void print_csv_header(void) {
    printf("task_id,name,priority,status,creation_time,start_time,end_time,duration_ms,timeout_seconds,retry_count,worker_id\n");
}

void print_task_csv(Task* task) {
    char creation_time[64], start_time[64], end_time[64];
    format_timestamp(task->creation_time, creation_time, sizeof(creation_time));
    if (task->start_time > 0) {
        format_timestamp(task->start_time, start_time, sizeof(start_time));
    } else {
        strcpy(start_time, "");
    }
    if (task->end_time > 0) {
        format_timestamp(task->end_time, end_time, sizeof(end_time));
    } else {
        strcpy(end_time, "");
    }
    
    unsigned int duration = 0;
    if (task->start_time > 0 && task->end_time > 0) {
        duration = (unsigned int)difftime(task->end_time, task->start_time) * 1000;
    } else if (task->start_time > 0) {
        time_t now = time(NULL);
        duration = (unsigned int)difftime(now, task->start_time) * 1000;
    }
    
    printf("%d,\"%s\",%s,%s,%s,%s,%s,%u,%u,%d,%d\n",
           task->id,
           task->name,
           priority_to_string(task->priority),
           status_to_string(task->status),
           creation_time,
           start_time,
           end_time,
           duration,
           task->timeout_seconds,
           task->retry_count,
           task->worker_id);
}

int main(void) {
    TaskQueue* queue = attach_shared_memory(-1);
    if (queue == NULL) {
        fprintf(stderr, "Error: Failed to attach to shared memory\n");
        return 1;
    }
    
    // Lock for safe reading
    pthread_mutex_lock(&queue->queue_mutex);
    
    // Print CSV header
    print_csv_header();
    
    // Print all tasks
    for (int i = 0; i < queue->size; i++) {
        print_task_csv(&queue->tasks[i]);
    }
    
    // Print summary statistics
    fprintf(stderr, "\nSummary:\n");
    fprintf(stderr, "Total Tasks: %d\n", queue->total_tasks);
    fprintf(stderr, "Completed: %d\n", queue->completed_tasks);
    fprintf(stderr, "Failed: %d\n", queue->failed_tasks);
    fprintf(stderr, "Pending: %d\n", get_pending_task_count(queue));
    fprintf(stderr, "Running: %d\n", get_running_task_count(queue));
    
    pthread_mutex_unlock(&queue->queue_mutex);
    detach_shared_memory(queue);
    return 0;
}
EOF
    gcc -o report_helper report_helper.c src/task_queue.c src/common.c -lpthread -I. || {
        echo "Error: Failed to compile report_helper"
        rm -f report_helper.c
        exit 1
    }
    rm -f report_helper.c
fi

# Generate report
echo "Generating report to $OUTPUT_FILE..."
./report_helper > "$OUTPUT_FILE" 2>&1

if [ $? -eq 0 ]; then
    echo "Report generated successfully: $OUTPUT_FILE"
    echo "Report contains $(wc -l < "$OUTPUT_FILE" | tr -d ' ') lines (including header)"
else
    echo "Error: Failed to generate report"
    exit 1
fi

