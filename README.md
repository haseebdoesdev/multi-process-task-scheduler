# Multi-Process Task Scheduler with Advanced Scheduling Algorithms

A comprehensive task scheduler that orchestrates multiple worker processes and their internal thread pools to execute heterogeneous tasks concurrently. This project demonstrates core Operating Systems concepts including process management, inter-process communication (IPC), threading, synchronization, and **multiple scheduling algorithms**.

## Overview

The system consists of:
- **Main Scheduler**: Parent process that manages a shared task queue and coordinates worker processes
- **Worker Processes**: Multiple worker processes that continuously poll the queue and execute tasks using threads with CPU affinity
- **Shared Memory**: Centralized task queue accessible by all processes with proper synchronization
- **Web Dashboard**: Real-time web interface with advanced visualization and interactive controls
- **Shell Interface**: Operator-friendly scripts for task management and monitoring

## Features

### ⚙️ Advanced Scheduling Algorithms

**9 Different Scheduling Algorithms:**
1. **Priority-Based** - Traditional priority scheduling (HIGH, MEDIUM, LOW)
2. **EDF (Earliest Deadline First)** - Deadline-aware scheduling for time-critical tasks
3. **MLFQ (Multi-Level Feedback Queue)** - Dynamic priority adjustment based on execution history
4. **Gang Scheduling** - Group-related tasks to execute together
5. **Round Robin (RR)** - Time-sliced circular scheduling with quantum
6. **SJF (Shortest Job First)** - Optimize for shortest tasks first
7. **FIFO/FCFS** - First In First Out / First Come First Served
8. **Lottery Scheduling** - Weighted random selection based on tickets
9. **SRTF (Shortest Remaining Time First)** - Preemptive shortest remaining time

### 🎯 Core Features

- **Multiple worker processes** (configurable, default: 3) with thread-based execution
- **CPU Affinity** - Workers pinned to specific CPU cores for cache optimization
- **Shared memory IPC** with mutex and condition variable synchronization
- **🌐 Beautiful Web Dashboard** with real-time updates (200ms refresh rate)
- **Real-time progress tracking** with millisecond-precision progress bars
- **Task cancellation** - Cancel pending tasks on demand
- **Export functionality** - Export task data as CSV or JSON
- **Terminal-based real-time monitoring**
- **🎯 Simulation Script** - Demonstrates all system mechanisms automatically
- **CSV/JSON report generation**
- **Graceful shutdown** and resource cleanup

## Project Structure

```
os-project/
├── src/
│   ├── scheduler.c      # Main scheduler process
│   ├── worker.c         # Worker process implementation (with CPU affinity)
│   ├── task_queue.c     # Shared memory queue operations (all algorithms)
│   ├── task_queue.h     # Task structures and queue definitions
│   ├── common.c         # Common utility functions
│   ├── common.h         # Common definitions (algorithms enum)
│   ├── logger.c         # Logging utility
│   ├── logger.h         # Logging header
│   └── web_server.c     # HTTP server for web dashboard
├── web/
│   ├── index.html       # Dashboard HTML
│   ├── dashboard.js     # Dashboard JavaScript (real-time updates)
│   └── dashboard.css    # Dashboard styling
├── scripts/
│   ├── start_scheduler.sh   # Start the scheduler
│   ├── start_web_dashboard.sh  # Start web dashboard
│   ├── stop_web_dashboard.sh   # Stop web dashboard
│   ├── add_task.sh          # Add a task to the queue
│   ├── monitor.sh           # Real-time monitoring
│   ├── report.sh            # Generate CSV reports
│   ├── runSimulation.sh     # Run automated simulations
│   └── cleanup.sh           # Cleanup resources
├── config.h             # Configuration constants
├── Makefile             # Build configuration
└── README.md            # This file
```

## Requirements

- **Linux environment** (tested on Ubuntu/Debian/WSL)
- **GCC compiler** with C11 support
- **POSIX-compliant system** with support for:
  - System V IPC (shared memory, message queues)
  - POSIX threads (pthreads)
  - Named pipes (FIFOs)
  - CPU affinity (`pthread_setaffinity_np`)
  - High-resolution timing (`clock_gettime`)
- **Modern web browser** for dashboard (Chrome, Firefox, Edge, Safari)

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

#### Via Command Line:
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

#### Via Web Dashboard (Recommended):
1. Open the dashboard at http://localhost:8080
2. Use the "Add Task" panel
3. Fill in task details:
   - **Name**: Task name
   - **Priority**: HIGH, MEDIUM, or LOW
   - **Duration**: Execution time in milliseconds
   - **Deadline** (optional): Seconds from now (for EDF algorithm)
   - **Gang ID** (optional): Group identifier (for Gang scheduling)

### Scheduling Algorithms

Switch between algorithms using the dashboard dropdown or via API:

**Available Algorithms:**
- **PRIORITY** - Traditional priority-based (default)
- **EDF** - Earliest Deadline First (requires deadlines)
- **MLFQ** - Multi-Level Feedback Queue (dynamic priorities)
- **GANG** - Gang Scheduling (requires gang IDs)
- **ROUND_ROBIN** - Time-sliced circular scheduling
- **SJF** - Shortest Job First
- **FIFO** - First In First Out
- **LOTTERY** - Lottery Scheduling (weighted random)
- **SRTF** - Shortest Remaining Time First

### Web Dashboard (Recommended)

Access the beautiful real-time web dashboard:
```bash
./scripts/start_web_dashboard.sh
```

Then open your browser and visit: **http://localhost:8080**

**Features:**
- 🎨 Modern, animated dashboard interface with dark theme
- 📊 Real-time graphs and charts (throughput, status distribution, worker utilization)
- 📈 Live task queue with **smooth progress bars** (millisecond precision)
- ⚡ Auto-refreshing statistics (200ms refresh rate - 5 updates/second)
- 🎯 **Interactive task management**:
  - Add tasks directly from dashboard (with optional deadline and gang ID)
  - Cancel pending tasks
  - View detailed task information in modal
  - Filter by status and priority
- 🔄 **Algorithm selector** - Switch scheduling algorithms in real-time
- 📤 **Export functionality** - Download task data as CSV or JSON
- 🎭 **Priority transition visualization** - See MLFQ priority changes in real-time
- ⏰ **Deadline countdown timers** - Visual countdown for EDF tasks
- 📱 Responsive design (works on mobile/tablet)

The dashboard updates every 200ms (5 times per second) automatically. Stop it with:
```bash
./scripts/stop_web_dashboard.sh
```

### Terminal Monitoring

Monitor the system in the terminal:
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

**Queue Settings:**
- `MAX_TASKS`: Maximum number of tasks in queue (default: 100)

**Worker Settings:**
- `NUM_WORKERS`: Number of worker processes (default: 3)
  - Each worker can run multiple tasks concurrently (limited by system resources)
  - Workers are pinned to CPU cores for optimal performance
- `MAX_THREADS_PER_WORKER`: Thread pool size per worker (default: 4) - Note: Currently informational

**Scheduling Configuration:**
- Round Robin time quantum: 2000ms (configurable in `task_queue.c`)
- MLFQ time slice: 1000ms (configurable in `task_queue.c`)
- MLFQ max CPU time before demotion: 5000ms

**IPC Keys:**
- `SHM_KEY`, `SEM_KEY`, `MSG_KEY`: IPC keys for shared memory, semaphores, and message queues

**Paths:**
- `LOG_DIR`: Logging directory (default: "logs")

After changing configuration, rebuild:
```bash
make clean && make
```

## Architecture Details

### Task Queue

Tasks are stored in a shared memory queue with algorithm-aware ordering. The queue maintains:
- **Algorithm-specific ordering** (priority, deadline, execution time, etc.)
- **Task lifecycle tracking** (PENDING → RUNNING → COMPLETED/FAILED)
- **Global statistics** (total, completed, failed tasks)
- **Advanced fields**:
  - Deadline times (for EDF)
  - Gang IDs (for Gang scheduling)
  - CPU time tracking (for MLFQ)
  - Lottery tickets (for Lottery scheduling)
  - Remaining time (for SRTF)

### Scheduling Algorithms Implementation

- **Priority/MLFQ**: Binary search insertion for O(log n) enqueue
- **EDF**: Deadline-sorted queue (earliest first)
- **Round Robin**: Circular index tracking with wrap-around
- **SJF/FIFO**: Linear search for optimal task
- **Lottery**: Weighted random selection with ticket system
- **SRTF**: Remaining time tracking and updates during execution
- **Gang**: Group-based coordination (tasks with same gang_id)

### Synchronization

- **Mutex**: Protects queue operations (enqueue/dequeue, status updates, algorithm changes)
- **Condition Variable**: Signals workers when tasks become available (no busy-waiting)
- **Process-shared attributes**: Mutex and condition variable are shared across processes
- **Thread-safe counters**: Safe access to task counts without holding mutex

### Worker Process Model

- **3 worker processes** run continuously in a pool (configurable)
- **CPU Affinity**: Each worker is pinned to a specific CPU core (round-robin assignment)
- **Efficient task waiting**: Uses condition variables instead of polling
- **Thread-based execution**: Each task runs in its own thread within the worker
- **Algorithm-aware dequeue**: Workers use the configured scheduling algorithm
- Worker processes are monitored and respawned if they crash

### Performance Optimizations

- **Binary search insertion** for priority-based algorithms (O(log n))
- **Condition variable waits** instead of busy polling
- **CPU affinity** for better cache performance
- **Millisecond-precision timing** for accurate progress tracking
- **Periodic cleanup** of completed tasks to prevent memory leaks

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

3. Start the web dashboard (in a new terminal):
```bash
./scripts/start_web_dashboard.sh
```
Then open http://localhost:8080 in your browser

Or monitor in terminal:
```bash
./scripts/monitor.sh
```

4. Run the simulation script (demonstrates all features):
```bash
./scripts/runSimulation.sh
```

This script demonstrates:
- **Priority-based scheduling** (HIGH tasks execute first, even if added later)
- **Concurrent execution** (multiple tasks in parallel across workers)
- **Queue management** (tasks queue when workers are busy)
- **Worker distribution** (tasks distributed across workers)
- **Burst load handling** (system handles sudden spikes)
- **Mixed workloads** (different task types and durations)
- **Long-running tasks** (quick vs long task comparison)
- **Continuous load** (steady-state behavior)

5. Generate a report:
```bash
./scripts/report.sh my_report.csv
```

6. Clean up:
```bash
./scripts/cleanup.sh
```

## Troubleshooting

### Scheduler won't start
- Check if shared memory already exists: `ipcs -m`
- Run cleanup script: `./scripts/cleanup.sh`
- Check logs in `logs/` directory
- Verify WSL/Linux environment is running

### Tasks not executing
- Verify workers are running: `ps aux | grep worker`
- Check worker logs in `logs/` directory
- Ensure shared memory is accessible
- Verify workers have CPU affinity set correctly

### Web dashboard issues
- Ensure scheduler is running before starting dashboard
- Check if port 8080 is available: `netstat -tlnp | grep 8080`
- View dashboard logs: `tail -f logs/web_server.log`
- Verify web files exist in `web/` directory

### Progress bar not updating
- Dashboard should update every 200ms automatically
- Check browser console for JavaScript errors
- Verify web server is responding: `curl http://localhost:8080/api/status`

### Permission errors
- Ensure scripts are executable: `chmod +x scripts/*.sh`
- Check write permissions for `logs/` directory
- In WSL, ensure files have correct line endings: `sed -i 's/\r$//' scripts/*.sh`

### Shared memory issues
- Run cleanup: `./scripts/cleanup.sh`
- Check system limits: `ipcs -l`
- Manually remove: `ipcrm -m <shmid>`
- In WSL, restart WSL service if IPC fails

### Algorithm switching not working
- Verify algorithm name is correct (case-sensitive)
- Check scheduler logs for algorithm change confirmation
- Ensure tasks are added after algorithm switch for best results

## Project Authors

- Raja Amar (FA23-BCS-086)
- Abdul Haseeb (FA23-BCS-120)
- Nouman Zahid (FA23-BCS-085)

## Course Information

**Course:** CSC323 - Operating System  
**Instructor:** Mr. Shahid Ali  
**Institution:** COMSATS University Islamabad  
**Date:** 11/17/25

## Technical Highlights

### Advanced Scheduling Features
- **9 Scheduling Algorithms** - Comprehensive coverage of OS scheduling concepts
- **Deadline-Aware Scheduling** - EDF implementation for real-time systems
- **Dynamic Priority Adjustment** - MLFQ with automatic priority demotion
- **CPU Affinity** - Worker-core binding for cache optimization
- **Time-Sliced Scheduling** - Round Robin with configurable quantum

### Performance Features
- **Millisecond-Precision Progress** - Real-time progress tracking with `clock_gettime()`
- **Efficient Queue Operations** - Binary search insertion (O(log n))
- **Condition Variable Waits** - No busy-waiting, efficient resource usage
- **Thread-Safe Counters** - Safe concurrent access without blocking
- **Memory Management** - Automatic cleanup of completed tasks

### Web Dashboard Features
- **200ms Refresh Rate** - 5 updates per second for smooth real-time updates
- **Interactive Controls** - Add tasks, cancel tasks, switch algorithms
- **Visual Indicators** - Progress bars, deadline countdowns, priority transitions
- **Export Capabilities** - CSV and JSON export for analysis
- **Responsive Design** - Works on desktop, tablet, and mobile

## Concurrent Execution Capacity

- **Worker Processes**: 3 (configurable via `NUM_WORKERS`)
- **Concurrent Tasks**: Up to 3 tasks can start simultaneously (one per worker)
- **Each worker** can execute multiple tasks concurrently via threads (limited by system resources)
- **Queue Capacity**: 100 tasks (configurable via `MAX_TASKS`)

## References

1. A. Silberschatz, P. B. Galvin, and G. Gagne, Operating System Concepts, 10th ed., Wiley, 2018.
2. The Austin Group, The Open Group Base Specifications Issue 7, 2018 Edition (POSIX.1-2017).
3. Linux manual pages: shmget(2), shmat(2), shmctl(2), msgget(2), pipe(2), pthreads(7), clock_gettime(2).
4. B. W. Kernighan and R. Pike, The Unix Programming Environment, Prentice Hall, 1984.

