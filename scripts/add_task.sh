#!/bin/bash

# Add Task Script
# Usage: ./add_task.sh <name> <priority> <duration_ms>
# Priority: HIGH, MEDIUM, or LOW
# Duration: execution time in milliseconds

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

if [ $# -lt 3 ]; then
    echo "Usage: $0 <name> <priority> <duration_ms>"
    echo "  name: Task name (use quotes if it contains spaces)"
    echo "  priority: HIGH, MEDIUM, or LOW"
    echo "  duration_ms: Execution time in milliseconds"
    echo ""
    echo "Example: $0 \"Data Processing\" HIGH 5000"
    exit 1
fi

TASK_NAME="$1"
PRIORITY_STR="$2"
DURATION_MS="$3"

# Validate priority
PRIORITY_NUM=-1
case "$PRIORITY_STR" in
    HIGH|high|High)
        PRIORITY_NUM=0
        ;;
    MEDIUM|medium|Medium)
        PRIORITY_NUM=1
        ;;
    LOW|low|Low)
        PRIORITY_NUM=2
        ;;
    *)
        echo "Error: Invalid priority. Must be HIGH, MEDIUM, or LOW"
        exit 1
        ;;
esac

# Validate duration
if ! [[ "$DURATION_MS" =~ ^[0-9]+$ ]]; then
    echo "Error: Duration must be a positive integer"
    exit 1
fi

# Check if scheduler is running
if [ ! -f scheduler.pid ]; then
    echo "Error: Scheduler is not running. Start it with ./scripts/start_scheduler.sh"
    exit 1
fi

PID=$(cat scheduler.pid)
if ! ps -p "$PID" > /dev/null 2>&1; then
    echo "Error: Scheduler is not running (stale PID file)"
    exit 1
fi

# Create named pipe if it doesn't exist
PIPE_PATH="/tmp/task_scheduler_pipe"
if [ ! -p "$PIPE_PATH" ]; then
    mkfifo "$PIPE_PATH" 2>/dev/null || {
        echo "Error: Failed to create task pipe"
        exit 1
    }
fi

# Create a helper program to add tasks via shared memory
# We'll use a simple C program for this
if [ ! -f add_task_helper ]; then
    cat > add_task_helper.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "config.h"
#include "src/task_queue.h"
#include "src/common.h"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <name> <priority> <duration>\n", argv[0]);
        return 1;
    }
    
    char* name = argv[1];
    int priority = atoi(argv[2]);
    unsigned int duration = (unsigned int)atoi(argv[3]);
    
    TaskQueue* queue = attach_shared_memory(-1);
    if (queue == NULL) {
        fprintf(stderr, "Error: Failed to attach to shared memory\n");
        return 1;
    }
    
    int task_id = enqueue_task(queue, name, priority, duration);
    if (task_id > 0) {
        printf("Task added successfully. ID: %d\n", task_id);
    } else {
        fprintf(stderr, "Error: Failed to add task (queue might be full)\n");
        detach_shared_memory(queue);
        return 1;
    }
    
    detach_shared_memory(queue);
    return 0;
}
EOF
    gcc -o add_task_helper add_task_helper.c src/task_queue.c src/common.c -lpthread -I. || {
        echo "Error: Failed to compile add_task_helper"
        rm -f add_task_helper.c
        exit 1
    }
    rm -f add_task_helper.c
fi

# Add the task
./add_task_helper "$TASK_NAME" "$PRIORITY_NUM" "$DURATION_MS"
RESULT=$?

if [ $RESULT -eq 0 ]; then
    echo "Task '$TASK_NAME' added with priority $PRIORITY_STR and duration ${DURATION_MS}ms"
fi

exit $RESULT

