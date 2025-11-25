#!/bin/bash

# Cleanup Script
# Removes shared memory segments and kills scheduler/worker processes

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

echo "Cleaning up scheduler resources..."

# Kill web server if running
if [ -f web_server.pid ]; then
    PID=$(cat web_server.pid)
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Killing web server (PID: $PID)..."
        kill -TERM "$PID" 2>/dev/null
        sleep 1
        if ps -p "$PID" > /dev/null 2>&1; then
            kill -KILL "$PID" 2>/dev/null
        fi
    fi
    rm -f web_server.pid
fi

# Kill scheduler if running
if [ -f scheduler.pid ]; then
    PID=$(cat scheduler.pid)
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Killing scheduler (PID: $PID)..."
        kill -TERM "$PID" 2>/dev/null
        sleep 2
        # Force kill if still running
        if ps -p "$PID" > /dev/null 2>&1; then
            kill -KILL "$PID" 2>/dev/null
        fi
    fi
    rm -f scheduler.pid
fi

# Kill any remaining worker processes
echo "Killing worker processes..."
pkill -f "worker [0-9]" 2>/dev/null
sleep 1
pkill -9 -f "worker [0-9]" 2>/dev/null

# Remove shared memory segments
echo "Removing shared memory segments..."
SHM_KEY=0x12345678
SHM_ID=$(ipcs -m | grep "$(printf '%x' $SHM_KEY)" | awk '{print $2}')

if [ -n "$SHM_ID" ]; then
    ipcrm -m "$SHM_ID" 2>/dev/null
    echo "Removed shared memory segment $SHM_ID"
else
    echo "No shared memory segment found"
fi

# Remove named pipe
if [ -p /tmp/task_scheduler_pipe ]; then
    rm -f /tmp/task_scheduler_pipe
    echo "Removed named pipe"
fi

# Clean up helper binaries
rm -f add_task_helper monitor_helper report_helper
rm -f add_task_helper.c monitor_helper.c report_helper.c

echo "Cleanup completed."

