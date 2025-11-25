# Multi-Process Task Scheduler with Priority Management

A priority-based task scheduler that orchestrates multiple worker processes and their internal thread pools to execute heterogeneous tasks concurrently. This project demonstrates core Operating Systems concepts including process management, inter-process communication (IPC), threading, and synchronization.

## Overview

The system consists of:
- **Main Scheduler**: Parent process that manages a shared task queue and coordinates worker processes
- **Worker Processes**: Multiple worker processes that continuously poll the queue and execute tasks using threads
- **Shared Memory**: Centralized task queue accessible by all processes with proper synchronization
- **Shell Interface**: Operator-friendly scripts for task management and monitoring

## Features

- Priority-based task scheduling (HIGH, MEDIUM, LOW)
- Multiple worker processes with thread-based execution
- Shared memory IPC with mutex and condition variable synchronization
- Real-time monitoring and logging
- CSV report generation
- Graceful shutdown and resource cleanup

## Project Structure

```
os-project/
├── src/
│   ├── scheduler.c      # Main scheduler process
│   ├── worker.c         # Worker process implementation
│   ├── task_queue.c     # Shared memory queue operations
│   ├── task_queue.h     # Task structures and queue definitions
│   ├── common.c         # Common utility functions
│   ├── common.h         # Common definitions
│   ├── logger.c         # Logging utility
│   └── logger.h         # Logging header
├── scripts/
│   ├── start_scheduler.sh   # Start the scheduler
│   ├── add_task.sh          # Add a task to the queue
│   ├── monitor.sh           # Real-time monitoring
│   ├── report.sh            # Generate CSV reports
│   └── cleanup.sh           # Cleanup resources
├── config.h             # Configuration constants
├── Makefile             # Build configuration
└── README.md            # This file
```

## Requirements

- Linux environment (tested on Ubuntu/Debian)
- GCC compiler
- POSIX-compliant system with support for:
  - System V IPC (shared memory, message queues)
  - POSIX threads (pthreads)
  - Named pipes (FIFOs)

## Building

1. Clone or extract the project:
```bash
cd os-project
```

2. Build the project:
```bash
make
```

This will compile all source files and create the `scheduler` and `worker` executables.

3. Make scripts executable (if needed):
```bash
chmod +x scripts/*.sh
```

## Usage

### Starting the Scheduler

```bash
./scripts/start_scheduler.sh
```

This will:
- Build the project if needed
- Clean up any existing shared memory
- Start the scheduler process
- Spawn worker processes

The scheduler will run in the background. Logs are written to the `logs/` directory.

### Adding Tasks

```bash
./scripts/add_task.sh <name> <priority> <duration_ms>
```

**Parameters:**
- `name`: Task name (use quotes if it contains spaces)
- `priority`: HIGH, MEDIUM, or LOW
- `duration_ms`: Execution time in milliseconds

**Examples:**
```bash
./scripts/add_task.sh "Data Processing" HIGH 5000
./scripts/add_task.sh "Backup Task" MEDIUM 10000
./scripts/add_task.sh "Low Priority Task" LOW 2000
```

### Monitoring

Monitor the system in real-time:
```bash
./scripts/monitor.sh
```

This displays:
- Total/completed/failed task counts
- Active worker count
- Current queue status
- All tasks with their status

Press Ctrl+C to exit monitoring.

### Generating Reports

Generate a CSV report of all tasks:
```bash
./scripts/report.sh [output_file.csv]
```

If no output file is specified, a timestamped file will be created.

**Report Format:**
- task_id
- name
- priority
- status
- creation_time
- start_time
- end_time
- duration_ms
- worker_id

### Cleanup

Stop the scheduler and clean up all resources:
```bash
./scripts/cleanup.sh
```

This will:
- Kill the scheduler and all worker processes
- Remove shared memory segments
- Clean up temporary files

## Configuration

Edit `config.h` to customize:

- `MAX_TASKS`: Maximum number of tasks in queue (default: 100)
- `NUM_WORKERS`: Number of worker processes (default: 3)
- `MAX_THREADS_PER_WORKER`: Thread pool size per worker (default: 4)
- `SHM_KEY`, `SEM_KEY`, `MSG_KEY`: IPC keys
- `LOG_DIR`: Logging directory (default: "logs")

After changing configuration, rebuild:
```bash
make clean && make
```

## Architecture Details

### Task Queue

Tasks are stored in a priority queue in shared memory. The queue maintains:
- Priority ordering (HIGH=0, MEDIUM=1, LOW=2)
- Task lifecycle tracking (PENDING → RUNNING → COMPLETED/FAILED)
- Global statistics (total, completed, failed tasks)

### Synchronization

- **Mutex**: Protects queue operations (enqueue/dequeue, status updates)
- **Condition Variable**: Signals workers when tasks become available
- **Process-shared attributes**: Mutex and condition variable are shared across processes

### Worker Process Model

- Workers run continuously in a pool
- Each worker polls the queue for tasks
- Tasks are executed in separate threads within the worker
- Worker processes are monitored and respawned if they crash

### Logging

All processes log to separate files in the `logs/` directory:
- `scheduler_<pid>.log`: Scheduler logs
- `worker_<id>_<pid>.log`: Worker logs

Log format: `[TIMESTAMP] [PID] [LEVEL] message`

## Example Workflow

1. Start the scheduler:
```bash
./scripts/start_scheduler.sh
```

2. Add several tasks:
```bash
./scripts/add_task.sh "Task 1" HIGH 3000
./scripts/add_task.sh "Task 2" LOW 5000
./scripts/add_task.sh "Task 3" HIGH 2000
./scripts/add_task.sh "Task 4" MEDIUM 4000
```

3. Monitor the execution:
```bash
./scripts/monitor.sh
```

4. Generate a report:
```bash
./scripts/report.sh my_report.csv
```

5. Clean up:
```bash
./scripts/cleanup.sh
```

## Troubleshooting

### Scheduler won't start
- Check if shared memory already exists: `ipcs -m`
- Run cleanup script: `./scripts/cleanup.sh`
- Check logs in `logs/` directory

### Tasks not executing
- Verify workers are running: `ps aux | grep worker`
- Check worker logs in `logs/` directory
- Ensure shared memory is accessible

### Permission errors
- Ensure scripts are executable: `chmod +x scripts/*.sh`
- Check write permissions for `logs/` directory

### Shared memory issues
- Run cleanup: `./scripts/cleanup.sh`
- Check system limits: `ipcs -l`
- Manually remove: `ipcrm -m <shmid>`

## Project Authors

- Raja Amar (FA23-BCS-086)
- Abdul Haseeb (FA23-BCS-120)
- Nouman Zahid (FA23-BCS-085)

## Course Information

**Course:** CSC323 - Operating System  
**Instructor:** Mr. Shahid Ali  
**Institution:** COMSATS University Islamabad  
**Date:** 11/17/25

## References

1. A. Silberschatz, P. B. Galvin, and G. Gagne, Operating System Concepts, 10th ed., Wiley, 2018.
2. The Austin Group, The Open Group Base Specifications Issue 7, 2018 Edition (POSIX.1-2017).
3. Linux manual pages: shmget(2), shmat(2), shmctl(2), msgget(2), pipe(2), pthreads(7).
4. B. W. Kernighan and R. Pike, The Unix Programming Environment, Prentice Hall, 1984.

