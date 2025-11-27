# Multi-Process Task Scheduler - Complete Project Overview

## 📋 Table of Contents
1. [Project Overview](#project-overview)
2. [Architecture & Components](#architecture--components)
3. [File Structure & Functions](#file-structure--functions)
4. [How Components Work Together](#how-components-work-together)
5. [Data Flow](#data-flow)
6. [Synchronization Mechanisms](#synchronization-mechanisms)

---

## 🎯 Project Overview

This is a **multi-process task scheduler** that demonstrates core Operating Systems concepts:
- **Process Management**: Multiple worker processes managed by a scheduler
- **Inter-Process Communication (IPC)**: Shared memory for task queue
- **Threading**: Each worker uses threads to execute tasks concurrently
- **Synchronization**: Mutexes and condition variables for thread-safe operations
- **Priority Scheduling**: Tasks are executed based on priority (HIGH, MEDIUM, LOW)

### Key Features
- Priority-based task scheduling
- Multiple worker processes with thread pools
- Shared memory IPC with proper synchronization
- Web dashboard for real-time monitoring
- Terminal-based monitoring
- CSV report generation
- Graceful shutdown and resource cleanup

---

## 🏗️ Architecture & Components

### System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Main Scheduler Process                    │
│  - Creates shared memory                                     │
│  - Spawns worker processes                                  │
│  - Monitors workers (respawns if crashed)                   │
│  - Cleans up completed tasks                                │
└─────────────────────────────────────────────────────────────┘
                          │
                          │ Shared Memory (TaskQueue)
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ Worker 0     │  │ Worker 1     │  │ Worker 2     │
│ - Thread 1   │  │ - Thread 1   │  │ - Thread 1   │
│ - Thread 2   │  │ - Thread 2   │  │ - Thread 2   │
│ - Thread 3   │  │ - Thread 3   │  │ - Thread 3   │
│ - Thread 4   │  │ - Thread 4   │  │ - Thread 4   │
└──────────────┘  └──────────────┘  └──────────────┘
        │                 │                 │
        └─────────────────┼─────────────────┘
                          │
                          ▼
                ┌─────────────────┐
                │  Web Server      │
                │  (Port 8080)     │
                └─────────────────┘
```

### Component Roles

1. **Scheduler Process** (`scheduler.c`)
   - Parent process that manages the entire system
   - Creates and initializes shared memory
   - Spawns worker processes
   - Monitors worker health and respawns if needed
   - Periodically cleans up old completed tasks

2. **Worker Processes** (`worker.c`)
   - Child processes spawned by scheduler
   - Continuously poll the shared task queue
   - Execute tasks using threads (thread pool)
   - Update task status in shared memory

3. **Web Server** (`web_server.c`)
   - HTTP server on port 8080
   - Serves dashboard HTML/CSS/JS
   - Provides REST API for task status
   - Reads from shared memory to display real-time data

4. **Shared Memory** (`task_queue.c`)
   - Central data structure accessible by all processes
   - Contains task queue, statistics, and synchronization primitives

---

## 📁 File Structure & Functions

### Configuration Files

#### `config.h`
**Purpose**: Central configuration file with all system constants

**Key Definitions**:
- `MAX_TASKS`: Maximum tasks in queue (100)
- `NUM_WORKERS`: Number of worker processes (3)
- `MAX_THREADS_PER_WORKER`: Threads per worker (4)
- `SHM_KEY`, `SEM_KEY`, `MSG_KEY`: IPC keys for shared memory
- `LOG_DIR`: Directory for log files ("logs")
- Timeout and interval values

---

### Core Source Files

#### `src/common.h` & `src/common.c`
**Purpose**: Common utilities and data structures shared across all modules

**Key Components**:
- **Enums**:
  - `Priority`: HIGH (0), MEDIUM (1), LOW (2)
  - `TaskStatus`: PENDING, RUNNING, COMPLETED, FAILED
- **Functions**:
  - `priority_to_string()`: Convert priority enum to string
  - `status_to_string()`: Convert status enum to string
  - `get_current_time()`: Get current timestamp
  - `format_timestamp()`: Format timestamp as string

**Used By**: All modules (scheduler, worker, web_server, task_queue)

---

#### `src/logger.h` & `src/logger.c`
**Purpose**: Logging system for all processes

**Key Features**:
- Creates log files in `logs/` directory
- Format: `[TIMESTAMP] [PID] [LEVEL] message`
- Log levels: DEBUG, INFO, WARN, ERROR
- Each process has its own log file:
  - `scheduler_<pid>.log`
  - `worker_<id>_<pid>.log`
  - `web_server_<pid>.log`

**Functions**:
- `init_logger(process_name)`: Initialize logger for a process
- `log_message(level, format, ...)`: Write log message
- `close_logger()`: Close log file

**Used By**: All processes for debugging and monitoring

---

#### `src/task_queue.h` & `src/task_queue.c`
**Purpose**: Shared memory task queue implementation

**Key Data Structures**:

1. **Task Structure**:
```c
typedef struct {
    int id;                    // Unique task ID
    char name[256];            // Task name
    Priority priority;         // HIGH, MEDIUM, or LOW
    TaskStatus status;         // PENDING, RUNNING, COMPLETED, FAILED
    time_t creation_time;      // When task was created
    time_t start_time;         // When task started executing
    time_t end_time;           // When task finished
    unsigned int execution_time_ms;  // Expected duration
    int worker_id;             // Which worker is executing it
    pthread_t thread_id;       // Thread executing the task
} Task;
```

2. **TaskQueue Structure** (in shared memory):
```c
typedef struct {
    Task tasks[MAX_TASKS];           // Array of tasks
    int size;                         // Current number of tasks
    int capacity;                     // Maximum capacity
    int next_task_id;                // Auto-incrementing ID
    int total_tasks;                  // Total tasks ever created
    int completed_tasks;              // Count of completed tasks
    int failed_tasks;                 // Count of failed tasks
    pthread_mutex_t queue_mutex;      // Mutex for synchronization
    pthread_cond_t queue_cond;        // Condition variable for signaling
    pid_t scheduler_pid;              // PID of scheduler process
    int num_active_workers;           // Number of active workers
    int shutdown_flag;                // Flag for graceful shutdown
} TaskQueue;
```

**Key Functions**:

1. **Shared Memory Management**:
   - `init_shared_memory()`: Creates shared memory segment
   - `attach_shared_memory(shm_id)`: Attach process to shared memory
   - `detach_shared_memory(queue)`: Detach from shared memory
   - `destroy_shared_memory(shm_id)`: Remove shared memory segment

2. **Task Operations**:
   - `enqueue_task()`: Add task to queue (priority-sorted insertion)
   - `dequeue_task()`: Remove highest-priority pending task
   - `update_task_status()`: Update task status (PENDING→RUNNING→COMPLETED/FAILED)
   - `find_task_by_id()`: Find task by ID

3. **Queue Queries**:
   - `is_queue_full()`: Check if queue is full
   - `is_queue_empty()`: Check if queue is empty
   - `get_pending_task_count()`: Count pending tasks
   - `get_running_task_count()`: Count running tasks
   - `remove_completed_tasks()`: Clean up old completed tasks

**Synchronization**:
- Uses `pthread_mutex_t` with `PTHREAD_PROCESS_SHARED` attribute
- Uses `pthread_cond_t` for signaling when tasks become available
- All queue operations are protected by mutex

**Used By**: Scheduler, workers, web server, helper scripts

---

#### `src/scheduler.c`
**Purpose**: Main scheduler process - orchestrates the entire system

**Main Responsibilities**:

1. **Initialization**:
   - Initialize logger
   - Set up signal handlers (SIGINT, SIGTERM)
   - Create shared memory segment
   - Write PID to `scheduler.pid` file

2. **Worker Management**:
   - Spawn `NUM_WORKERS` worker processes using `fork()` and `execl()`
   - Store worker PIDs in array
   - Monitor workers periodically (every 5 seconds)
   - Respawn workers if they crash

3. **Main Loop** (`monitor_workers()`):
   - Check worker health using `waitpid()` with `WNOHANG`
   - Update active worker count in shared memory
   - Periodically clean up old completed tasks (every 60 seconds)

4. **Cleanup**:
   - Send SIGTERM to all workers
   - Wait for workers to finish
   - Detach from shared memory
   - Close logger

**Key Functions**:
- `spawn_worker(worker_id)`: Fork and exec a worker process
- `monitor_workers()`: Main monitoring loop
- `signal_handler()`: Handle shutdown signals
- `cleanup_resources()`: Clean up on exit

**Process Flow**:
```
main()
  ├─> init_logger()
  ├─> signal handlers
  ├─> init_shared_memory()
  ├─> attach_shared_memory()
  ├─> spawn_worker() × NUM_WORKERS
  └─> monitor_workers() [loop until shutdown]
```

---

#### `src/worker.c`
**Purpose**: Worker process that executes tasks

**Main Responsibilities**:

1. **Initialization**:
   - Get worker ID from command line argument
   - Initialize logger
   - Set up signal handlers
   - Attach to shared memory
   - Register as active worker

2. **Main Loop** (`worker_main_loop()`):
   - Wait on condition variable for tasks (efficient blocking)
   - When task available, lock mutex and dequeue
   - Execute task in separate thread
   - Loop continues until shutdown

3. **Task Execution**:
   - `execute_task()`: Creates a thread to run the task
   - `task_executor_thread()`: Thread function that:
     - Simulates work by sleeping for `execution_time_ms`
     - Updates task status to COMPLETED
     - Updates task status to FAILED on error

**Key Functions**:
- `worker_main_loop()`: Main worker loop
- `execute_task()`: Spawn thread to execute task
- `task_executor_thread()`: Thread function that does the actual work
- `signal_handler()`: Handle shutdown signals

**Process Flow**:
```
main(worker_id)
  ├─> init_logger()
  ├─> attach_shared_memory()
  ├─> register as active worker
  └─> worker_main_loop() [loop until shutdown]
        ├─> wait on condition variable
        ├─> dequeue task
        └─> execute_task() → spawn thread
```

**Thread Model**:
- Each worker can run up to `MAX_THREADS_PER_WORKER` (4) tasks concurrently
- Threads are detached (no need to join)
- Threads update task status in shared memory when done

---

#### `src/web_server.c`
**Purpose**: HTTP server for web dashboard

**Main Responsibilities**:

1. **HTTP Server**:
   - Creates TCP socket on port 8080
   - Listens for connections
   - Handles requests synchronously

2. **API Endpoints**:
   - `GET /api/status`: Returns JSON with system statistics
   - `GET /api/tasks`: Returns JSON array of all tasks
   - `GET /api/workers`: Returns JSON with worker information

3. **Static File Serving**:
   - `GET /` or `/index.html`: Serves `web/index.html`
   - `GET /dashboard.js`: Serves `web/dashboard.js`
   - `GET /dashboard.css`: Serves `web/dashboard.css`

4. **Data Generation**:
   - Reads from shared memory (with mutex lock)
   - Generates JSON responses
   - Calculates task progress for running tasks

**Key Functions**:
- `handle_request()`: Parse HTTP request and route
- `handle_api_request()`: Handle API endpoints
- `serve_file()`: Serve static files
- `generate_status_json()`: Generate status JSON
- `generate_tasks_json()`: Generate tasks array JSON
- `generate_workers_json()`: Generate workers JSON

**Process Flow**:
```
main()
  ├─> init_logger()
  ├─> attach_shared_memory()
  ├─> create socket
  ├─> bind to port 8080
  ├─> listen()
  └─> accept() loop
        └─> handle_request()
              ├─> API request → handle_api_request()
              └─> Static file → serve_file()
```

---

### Web Dashboard Files

#### `web/index.html`
**Purpose**: Frontend HTML for dashboard

**Features**:
- Real-time statistics cards (Total, Running, Pending, Completed, Failed, Workers)
- Interactive charts (Task Throughput, Status Distribution) using Chart.js
- Task queue table with filtering and sorting
- Auto-refresh every 2 seconds
- Responsive design

#### `web/dashboard.js`
**Purpose**: JavaScript for dashboard functionality

**Key Features**:
- Fetches data from `/api/status`, `/api/tasks`, `/api/workers`
- Updates UI every 2 seconds
- Creates and updates charts
- Handles filtering and sorting
- Calculates and displays task progress

#### `web/dashboard.css`
**Purpose**: Styling for dashboard

**Features**:
- Modern, animated UI
- Color-coded status indicators
- Responsive grid layout
- Smooth transitions and animations

---

### Scripts

#### `scripts/start_scheduler.sh`
**Purpose**: Start the scheduler process

**What it does**:
1. Checks if scheduler already running
2. Creates `logs/` directory
3. Builds project if needed (`make`)
4. Cleans up existing shared memory
5. Starts scheduler in background
6. Verifies scheduler started successfully

**Usage**: `./scripts/start_scheduler.sh`

---

#### `scripts/add_task.sh`
**Purpose**: Add a task to the queue

**What it does**:
1. Validates arguments (name, priority, duration)
2. Checks if scheduler is running
3. Compiles `add_task_helper` if needed (C program that accesses shared memory)
4. Calls helper to add task via shared memory

**Usage**: `./scripts/add_task.sh "Task Name" HIGH 5000`

**Example**:
```bash
./scripts/add_task.sh "Data Processing" HIGH 5000
./scripts/add_task.sh "Backup Task" MEDIUM 10000
./scripts/add_task.sh "Low Priority Task" LOW 2000
```

---

#### `scripts/monitor.sh`
**Purpose**: Terminal-based real-time monitoring

**What it does**:
1. Checks if scheduler is running
2. Compiles `monitor_helper` if needed
3. Continuously displays:
   - System statistics
   - Task queue status
   - All tasks with details
4. Refreshes every 2 seconds

**Usage**: `./scripts/monitor.sh`

---

#### `scripts/report.sh`
**Purpose**: Generate CSV report of all tasks

**What it does**:
1. Compiles `report_helper` if needed
2. Reads all tasks from shared memory
3. Generates CSV with columns:
   - task_id, name, priority, status
   - creation_time, start_time, end_time
   - duration_ms, worker_id
4. Saves to file (default: timestamped filename)

**Usage**: `./scripts/report.sh [output_file.csv]`

---

#### `scripts/cleanup.sh`
**Purpose**: Stop scheduler and clean up resources

**What it does**:
1. Kills web server if running
2. Kills scheduler process
3. Kills all worker processes
4. Removes shared memory segments
5. Removes named pipe
6. Cleans up helper binaries

**Usage**: `./scripts/cleanup.sh`

---

#### `scripts/start_web_dashboard.sh`
**Purpose**: Start the web server

**What it does**:
1. Checks if scheduler is running (warns if not)
2. Builds web server if needed
3. Checks if web server already running
4. Starts web server in background
5. Displays URL and instructions

**Usage**: `./scripts/start_web_dashboard.sh`

**Access**: Open http://localhost:8080 in browser

---

#### `scripts/stop_web_dashboard.sh`
**Purpose**: Stop the web server

**What it does**:
1. Reads PID from `web_server.pid`
2. Kills web server process
3. Removes PID file

**Usage**: `./scripts/stop_web_dashboard.sh`

---

### Build System

#### `Makefile`
**Purpose**: Build configuration

**Targets**:
- `make` or `make all`: Build all executables (scheduler, worker, web_server)
- `make clean`: Remove build artifacts
- `make distclean`: Clean + remove logs and shared memory
- `make debug`: Build with debug symbols
- `make release`: Build optimized release

**Compilation**:
- Compiler: `gcc`
- Flags: `-Wall -Wextra -g -std=c11 -pthread -D_GNU_SOURCE`
- Libraries: `-lpthread -lrt`
- Output: `scheduler`, `worker`, `web_server` executables

---

## 🔄 How Components Work Together

### Startup Sequence

1. **User runs**: `./scripts/start_scheduler.sh`
2. **Script**:
   - Builds project (`make`)
   - Cleans up old shared memory
   - Starts `./scheduler` process
3. **Scheduler Process**:
   - Creates shared memory segment
   - Initializes TaskQueue structure
   - Spawns 3 worker processes
   - Enters monitoring loop
4. **Worker Processes**:
   - Attach to shared memory
   - Register as active workers
   - Enter main loop, waiting for tasks

### Task Execution Flow

1. **User adds task**: `./scripts/add_task.sh "Task" HIGH 5000`
2. **Script**:
   - Compiles `add_task_helper` if needed
   - Helper attaches to shared memory
   - Calls `enqueue_task()`:
     - Locks mutex
     - Inserts task in priority order (HIGH before MEDIUM before LOW)
     - Increments counters
     - Signals condition variable
     - Unlocks mutex
3. **Worker Process**:
   - Waiting on condition variable (blocked)
   - Wakes up when signaled
   - Locks mutex
   - Finds highest-priority pending task
   - Updates status to RUNNING
   - Unlocks mutex
   - Spawns thread to execute task
4. **Worker Thread**:
   - Sleeps for `execution_time_ms` (simulates work)
   - Updates task status to COMPLETED
   - Thread exits

### Web Dashboard Flow

1. **User starts dashboard**: `./scripts/start_web_dashboard.sh`
2. **Web Server**:
   - Attaches to shared memory
   - Starts HTTP server on port 8080
3. **Browser**:
   - Requests `http://localhost:8080`
   - Server responds with HTML
   - HTML loads CSS and JavaScript
4. **JavaScript**:
   - Every 2 seconds, fetches:
     - `/api/status` → Updates statistics cards
     - `/api/tasks` → Updates task table and charts
     - `/api/workers` → Updates worker count
   - Server reads from shared memory (with mutex lock)
   - Returns JSON
   - JavaScript updates UI

### Shutdown Sequence

1. **User runs**: `./scripts/cleanup.sh` or sends SIGTERM
2. **Scheduler**:
   - Sets `shutdown_flag = 1` in shared memory
   - Broadcasts condition variable (wakes all workers)
   - Sends SIGTERM to all workers
   - Waits for workers to finish
   - Detaches from shared memory
3. **Workers**:
   - See `shutdown_flag` set
   - Exit main loop
   - Unregister as active workers
   - Detach from shared memory
   - Exit
4. **Cleanup Script**:
   - Removes shared memory segment
   - Removes named pipe
   - Cleans up helper binaries

---

## 📊 Data Flow

### Task Lifecycle

```
PENDING → RUNNING → COMPLETED
              ↓
           FAILED
```

1. **PENDING**: Task in queue, waiting to be executed
2. **RUNNING**: Worker picked up task, thread executing it
3. **COMPLETED**: Task finished successfully
4. **FAILED**: Task encountered an error

### Priority Queue Structure

Tasks are stored in a **priority-sorted array**:
- HIGH priority tasks (priority=0) come first
- MEDIUM priority tasks (priority=1) come next
- LOW priority tasks (priority=2) come last

When dequeuing, workers always take the **first pending task** (highest priority).

### Shared Memory Layout

```
┌─────────────────────────────────────┐
│ TaskQueue Structure                 │
├─────────────────────────────────────┤
│ tasks[MAX_TASKS]                    │ ← Array of tasks (priority-sorted)
│ size                                │ ← Current number of tasks
│ capacity                            │ ← MAX_TASKS
│ next_task_id                        │ ← Auto-incrementing ID
│ total_tasks                         │ ← Total ever created
│ completed_tasks                     │ ← Count of completed
│ failed_tasks                        │ ← Count of failed
│ queue_mutex                         │ ← Mutex for synchronization
│ queue_cond                          │ ← Condition variable
│ scheduler_pid                       │ ← PID of scheduler
│ num_active_workers                  │ ← Active worker count
│ shutdown_flag                       │ ← Shutdown signal
└─────────────────────────────────────┘
```

---

## 🔒 Synchronization Mechanisms

### Mutex (Mutual Exclusion)

**Purpose**: Prevent race conditions when multiple processes/threads access shared memory

**Usage**:
- All queue operations (enqueue, dequeue, status update) are protected by mutex
- Mutex is process-shared (`PTHREAD_PROCESS_SHARED`) so it works across processes

**Example**:
```c
pthread_mutex_lock(&queue->queue_mutex);
// ... modify queue ...
pthread_mutex_unlock(&queue->queue_mutex);
```

### Condition Variable

**Purpose**: Efficiently wake up workers when tasks become available

**Usage**:
- Workers wait on condition variable when queue is empty
- When task is enqueued, condition variable is signaled
- Workers wake up and check for tasks

**Example**:
```c
// Worker waiting
pthread_mutex_lock(&queue->queue_mutex);
while (is_queue_empty(queue) && !shutdown_flag) {
    pthread_cond_wait(&queue->queue_cond, &queue->queue_mutex);
}
// ... dequeue task ...
pthread_mutex_unlock(&queue->queue_mutex);

// Scheduler/Enqueuer signaling
pthread_mutex_lock(&queue->queue_mutex);
enqueue_task(...);
pthread_cond_signal(&queue->queue_cond);  // Wake one worker
pthread_mutex_unlock(&queue->queue_mutex);
```

### Process-Shared Attributes

Both mutex and condition variable use `PTHREAD_PROCESS_SHARED` attribute so they work across different processes (not just threads).

---

## 🎓 Key OS Concepts Demonstrated

1. **Process Management**: Fork, exec, process monitoring, signal handling
2. **Inter-Process Communication**: System V shared memory
3. **Threading**: POSIX threads, thread pools, detached threads
4. **Synchronization**: Mutexes, condition variables, process-shared primitives
5. **Priority Scheduling**: Priority-based task queue
6. **Resource Management**: Shared memory lifecycle, cleanup

---

## 📝 Summary

This project is a **complete multi-process task scheduler** that demonstrates:
- How multiple processes can share data via shared memory
- How to synchronize access using mutexes and condition variables
- How to manage worker processes (spawn, monitor, respawn)
- How to execute tasks concurrently using threads
- How to provide a web interface for monitoring

The system is designed to be **robust** (workers can crash and be respawned), **efficient** (condition variables instead of polling), and **user-friendly** (web dashboard and scripts).


