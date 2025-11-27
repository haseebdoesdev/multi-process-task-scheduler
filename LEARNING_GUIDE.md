# Learning Guide - Understanding the Project

## 📖 How to Use This Guide

This guide will help you understand the project step by step. Start with the overview, then dive into specific components.

---

## 🎯 Step 1: Read the Overview

**Start here:** `PROJECT_OVERVIEW.md`

This document explains:
- What the project does
- How all components work together
- The architecture and data flow
- Each file's purpose

**Key Concepts to Understand:**
1. **Multi-process architecture**: Scheduler spawns worker processes
2. **Shared memory**: All processes share the same task queue
3. **Synchronization**: Mutexes and condition variables prevent race conditions
4. **Priority scheduling**: HIGH priority tasks execute before LOW priority
5. **Thread pools**: Each worker uses threads to execute tasks concurrently

---

## 🔍 Step 2: Understand the Core Components

### A. Start with Data Structures

**File:** `src/task_queue.h`

Understand these structures:
- `Task`: Represents a single task
- `TaskQueue`: The shared memory structure

**Key Questions:**
- What information does a Task contain?
- How is the queue organized? (Priority-sorted array)
- What synchronization primitives are used?

### B. Understand Shared Memory

**File:** `src/task_queue.c`

Focus on:
- `init_shared_memory()`: How shared memory is created
- `attach_shared_memory()`: How processes connect to it
- Process-shared mutex and condition variable initialization

**Key Questions:**
- Why do we need process-shared attributes?
- How do multiple processes access the same memory?

### C. Understand the Scheduler

**File:** `src/scheduler.c`

Trace the execution:
1. Initialization (shared memory, signal handlers)
2. Worker spawning (`spawn_worker()`)
3. Monitoring loop (`monitor_workers()`)

**Key Questions:**
- How does the scheduler spawn workers?
- What happens if a worker crashes?
- How does cleanup work?

### D. Understand Workers

**File:** `src/worker.c`

Trace the execution:
1. Worker initialization
2. Main loop (`worker_main_loop()`)
3. Task execution (`execute_task()`, `task_executor_thread()`)

**Key Questions:**
- How do workers wait for tasks? (Condition variable)
- How are tasks executed? (Threads)
- How is task status updated?

### E. Understand the Web Server

**File:** `src/web_server.c`

Focus on:
- HTTP server setup
- API endpoints (`/api/status`, `/api/tasks`, `/api/workers`)
- How it reads from shared memory

**Key Questions:**
- How does the web server get data?
- What API endpoints are available?
- How is JSON generated?

---

## 🧪 Step 3: Trace Through an Example

### Example: Adding and Executing a Task

**1. User adds task:**
```bash
./scripts/add_task.sh "Data Processing" HIGH 5000
```

**What happens:**
- Script validates arguments
- Compiles `add_task_helper` if needed
- Helper attaches to shared memory
- Calls `enqueue_task()`:
  - Locks mutex
  - Inserts task in priority order
  - Signals condition variable
  - Unlocks mutex

**2. Worker picks up task:**
- Worker is waiting on condition variable
- Wakes up when signaled
- Locks mutex
- Finds highest-priority pending task
- Updates status to RUNNING
- Unlocks mutex
- Spawns thread to execute

**3. Thread executes task:**
- Sleeps for 5000ms (simulates work)
- Updates status to COMPLETED
- Thread exits

**4. User views status:**
- Web dashboard fetches `/api/tasks`
- Server reads from shared memory
- Returns JSON
- Dashboard displays task as completed

---

## 🔬 Step 4: Experiment

### Experiment 1: Priority Ordering

Add tasks with different priorities and observe execution order:

```bash
./scripts/add_task.sh "Task A" LOW 2000
./scripts/add_task.sh "Task B" HIGH 2000
./scripts/add_task.sh "Task C" MEDIUM 2000
```

**Expected:** Task B (HIGH) executes first, then Task C (MEDIUM), then Task A (LOW)

### Experiment 2: Concurrent Execution

Add multiple tasks and watch them execute concurrently:

```bash
./scripts/add_task.sh "Task 1" HIGH 5000
./scripts/add_task.sh "Task 2" HIGH 5000
./scripts/add_task.sh "Task 3" HIGH 5000
./scripts/add_task.sh "Task 4" HIGH 5000
```

**Expected:** Multiple tasks run simultaneously (one per worker thread)

### Experiment 3: Worker Monitoring

Watch what happens when a worker crashes:

```bash
# Start scheduler
./scripts/start_scheduler.sh

# Find a worker PID
ps aux | grep worker

# Kill a worker
kill -9 <worker_pid>

# Monitor - worker should be respawned
./scripts/monitor.sh
```

**Expected:** Scheduler detects worker death and respawns it

---

## 📚 Step 5: Study OS Concepts

This project demonstrates:

### 1. Process Management
- **Fork/Exec**: `scheduler.c` uses `fork()` and `execl()` to spawn workers
- **Process Monitoring**: `waitpid()` with `WNOHANG` to check worker health
- **Signal Handling**: `SIGINT`, `SIGTERM` for graceful shutdown

**Study:** `man fork`, `man exec`, `man waitpid`, `man signal`

### 2. Inter-Process Communication (IPC)
- **Shared Memory**: System V shared memory (`shmget`, `shmat`, `shmdt`)
- **Process-Shared Synchronization**: Mutexes and condition variables

**Study:** `man shmget`, `man shmat`, `man pthread_mutexattr_setpshared`

### 3. Threading
- **Thread Creation**: `pthread_create()` in workers
- **Detached Threads**: `pthread_detach()` for fire-and-forget execution
- **Thread Safety**: Mutex-protected critical sections

**Study:** `man pthread_create`, `man pthread_mutex_lock`

### 4. Synchronization
- **Mutex**: Protects shared data structures
- **Condition Variables**: Efficient waiting instead of polling

**Study:** `man pthread_mutex_init`, `man pthread_cond_wait`

### 5. Priority Scheduling
- **Priority Queue**: Tasks sorted by priority
- **Preemptive Scheduling**: Higher priority tasks execute first

**Study:** Operating System scheduling algorithms

---

## 🛠️ Step 6: Modify and Experiment

### Easy Modifications

1. **Change number of workers:**
   - Edit `config.h`: `NUM_WORKERS`
   - Rebuild: `make clean && make`
   - Restart scheduler

2. **Change thread pool size:**
   - Edit `config.h`: `MAX_THREADS_PER_WORKER`
   - Rebuild and restart

3. **Change queue size:**
   - Edit `config.h`: `MAX_TASKS`
   - Rebuild and restart

### Advanced Modifications

1. **Add a new priority level:**
   - Modify `Priority` enum in `src/common.h`
   - Update `priority_to_string()` function
   - Update scripts to accept new priority

2. **Add task cancellation:**
   - Add `STATUS_CANCELLED` to `TaskStatus` enum
   - Implement cancellation logic in workers
   - Add API endpoint to cancel tasks

3. **Add task dependencies:**
   - Add dependency field to `Task` structure
   - Modify scheduler to check dependencies before execution

---

## 📊 Step 7: Analyze Performance

### Metrics to Observe

1. **Throughput**: Tasks completed per second
2. **Latency**: Time from task creation to completion
3. **Resource Usage**: CPU, memory usage of processes

### Tools

```bash
# Monitor processes
top -p $(pgrep -d, scheduler worker)

# Monitor shared memory
watch -n 1 'ipcs -m'

# Check logs
tail -f logs/scheduler_*.log
tail -f logs/worker_*.log
```

---

## 🎓 Step 8: Answer These Questions

Test your understanding:

1. **Why is shared memory needed?**
   - Answer: Processes have separate address spaces. Shared memory allows them to share data.

2. **Why use process-shared mutex instead of regular mutex?**
   - Answer: Regular mutexes only work within a single process. Process-shared mutexes work across processes.

3. **Why use condition variables instead of polling?**
   - Answer: Polling wastes CPU. Condition variables block until signaled, saving resources.

4. **What happens if two workers try to dequeue at the same time?**
   - Answer: Mutex ensures only one can access the queue at a time. The other waits.

5. **How does priority scheduling work?**
   - Answer: Tasks are inserted in priority order. Workers always take the first pending task (highest priority).

6. **Why use threads instead of executing tasks directly in workers?**
   - Answer: Threads allow concurrent execution. A worker can run multiple tasks simultaneously.

7. **What happens if the scheduler crashes?**
   - Answer: Workers would continue running but no new tasks could be added. Shared memory would remain.

8. **How does the web server get real-time data?**
   - Answer: It reads from shared memory (with mutex lock) and serves JSON. JavaScript polls the API every 2 seconds.

---

## 🔗 Step 9: Related Topics to Explore

1. **Message Queues**: Alternative to shared memory
2. **Semaphores**: Another synchronization primitive
3. **Process Pools**: Pre-forked worker pools
4. **Load Balancing**: Distributing tasks across workers
5. **Fault Tolerance**: Handling failures gracefully
6. **Distributed Systems**: Scaling across multiple machines

---

## 📝 Summary

**Learning Path:**
1. ✅ Read PROJECT_OVERVIEW.md
2. ✅ Understand data structures (Task, TaskQueue)
3. ✅ Trace through code (scheduler → worker → task execution)
4. ✅ Run experiments
5. ✅ Modify code
6. ✅ Study OS concepts
7. ✅ Answer questions

**Key Takeaways:**
- Multi-process systems require IPC (shared memory)
- Synchronization is critical (mutexes, condition variables)
- Priority scheduling ensures important tasks execute first
- Thread pools enable concurrent execution
- Monitoring and logging are essential for debugging

---

## 🎯 Next Steps

1. **Run the project** (see QUICK_START.md)
2. **Read the code** (see PROJECT_OVERVIEW.md)
3. **Experiment** (try the experiments above)
4. **Modify** (make your own changes)
5. **Learn** (study the OS concepts)

Happy learning! 🚀


