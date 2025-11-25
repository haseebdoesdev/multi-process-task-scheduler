#!/bin/bash

# Start Scheduler Script
# This script initializes and starts the scheduler process

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

# Check if scheduler is already running
if [ -f scheduler.pid ]; then
    PID=$(cat scheduler.pid)
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Error: Scheduler is already running with PID $PID"
        exit 1
    else
        echo "Removing stale PID file"
        rm -f scheduler.pid
    fi
fi

# Create logs directory
mkdir -p logs

# Build the project if needed
if [ ! -f scheduler ] || [ ! -f worker ]; then
    echo "Building project..."
    make clean
    make
    if [ $? -ne 0 ]; then
        echo "Error: Build failed"
        exit 1
    fi
fi

# Check if shared memory already exists and clean it up
ipcs -m | grep -q "0x$(printf '%x' 0x12345678)"
if [ $? -eq 0 ]; then
    echo "Warning: Shared memory segment exists. Cleaning up..."
    ./scripts/cleanup.sh 2>/dev/null
    sleep 1
fi

# Start scheduler in background
echo "Starting scheduler..."
./scheduler > logs/scheduler_startup.log 2>&1 &

# Wait a moment for scheduler to start
sleep 2

# Check if scheduler started successfully
if [ -f scheduler.pid ]; then
    PID=$(cat scheduler.pid)
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Scheduler started successfully with PID $PID"
        echo "Logs are in the logs/ directory"
    else
        echo "Error: Scheduler failed to start. Check logs/scheduler_startup.log"
        exit 1
    fi
else
    echo "Error: Scheduler PID file not created. Check logs/scheduler_startup.log"
    exit 1
fi

echo "Scheduler is now running. Use ./scripts/add_task.sh to add tasks."
echo "Use ./scripts/monitor.sh to monitor the system."

