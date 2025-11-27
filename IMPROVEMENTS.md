# Project Improvements - Task Recovery & Timeout

## ✅ Implemented Features

### 1. Task Recovery on Worker Crash (CRITICAL)

**Problem Solved:**
- Tasks stuck in RUNNING state when worker crashes
- No automatic recovery mechanism
- Data loss and queue blocking

**Implementation:**
- Added `retry_count` field to Task structure
- Implemented `recover_orphaned_tasks()` function
- Scheduler detects worker crashes and recovers orphaned tasks
- Tasks reset to PENDING for retry (up to MAX_TASK_RETRIES)
- Tasks exceeding max retries marked as FAILED

**How It Works:**
1. Scheduler monitors workers every 5 seconds
2. On worker crash detection, calls `recover_orphaned_tasks()`
3. Finds all RUNNING tasks assigned to dead worker
4. Resets them to PENDING if retry count < MAX_TASK_RETRIES
5. Marks as FAILED if max retries exceeded

**Configuration:**
- `MAX_TASK_RETRIES = 3` (in config.h)

---

### 2. Task Timeout Mechanism (CRITICAL)

**Problem Solved:**
- Tasks can run indefinitely if they hang
- No way to detect/kill stuck tasks
- System can become unresponsive

**Implementation:**
- Added `timeout_seconds` field to Task structure
- Added `STATUS_TIMEOUT` to TaskStatus enum
- Implemented `check_and_handle_timeouts()` function
- Scheduler checks for timeouts every 2 seconds
- Worker threads check timeout during execution
- Timed-out tasks reset for retry or marked as TIMEOUT

**How It Works:**
1. Each task can have a timeout (0 = no timeout)
2. Scheduler checks running tasks every 2 seconds
3. If task exceeds timeout:
   - Reset to PENDING for retry (if retries < MAX)
   - Mark as TIMEOUT (if max retries exceeded)
4. Worker threads also check timeout during execution

**Configuration:**
- `DEFAULT_TASK_TIMEOUT_SECONDS = 300` (5 minutes)
- `TASK_TIMEOUT_CHECK_INTERVAL = 2` seconds

---

## 📝 Changes Made

### Core Structures

**src/common.h:**
- Added `STATUS_TIMEOUT` to TaskStatus enum

**src/common.c:**
- Updated `status_to_string()` to handle STATUS_TIMEOUT

**src/task_queue.h:**
- Added `timeout_seconds` field to Task structure
- Added `retry_count` field to Task structure
- Updated `enqueue_task()` signature to accept timeout
- Added `recover_orphaned_tasks()` function
- Added `check_and_handle_timeouts()` function

**config.h:**
- Added `DEFAULT_TASK_TIMEOUT_SECONDS = 300`
- Added `MAX_TASK_RETRIES = 3`
- Added `TASK_TIMEOUT_CHECK_INTERVAL = 2`

### Implementation Files

**src/task_queue.c:**
- Updated `enqueue_task()` to accept and store timeout
- Initialize `retry_count = 0` for new tasks
- Updated `update_task_status()` to handle STATUS_TIMEOUT
- Updated `remove_completed_tasks()` to handle STATUS_TIMEOUT
- Implemented `recover_orphaned_tasks()` function
- Implemented `check_and_handle_timeouts()` function

**src/scheduler.c:**
- Updated `monitor_workers()` to:
  - Call `recover_orphaned_tasks()` on worker crash
  - Call `check_and_handle_timeouts()` every 2 seconds

**src/worker.c:**
- Updated `task_executor_thread()` to:
  - Check timeout during execution
  - Abort if timeout exceeded

### Scripts & Tools

**scripts/add_task.sh:**
- Added optional `timeout_seconds` parameter
- Default timeout: 300 seconds (5 minutes)
- Use `0` for no timeout
- Updated helper C program to pass timeout

**scripts/monitor.sh:**
- Display retry count and timeout in task list

**scripts/report.sh:**
- Include timeout_seconds and retry_count in CSV

**src/web_server.c:**
- Show timeout_seconds and retry_count in JSON
- Show timeout_tasks count in status JSON

---

## 🚀 Usage

### Adding Tasks with Timeout

```bash
# Default timeout (300 seconds)
./scripts/add_task.sh "Task Name" HIGH 5000

# Custom timeout (60 seconds)
./scripts/add_task.sh "Task Name" HIGH 5000 60

# No timeout
./scripts/add_task.sh "Task Name" HIGH 5000 0
```

### Monitoring

The scheduler automatically:
- Detects worker crashes and recovers tasks
- Monitors task timeouts every 2 seconds
- Retries failed tasks (up to 3 times)
- Logs all recovery and timeout events

### Viewing in Dashboard

The web dashboard now shows:
- Timeout status for each task
- Retry count for each task
- Separate timeout task count in statistics

---

## 🔍 Testing

### Test Worker Crash Recovery

1. Start scheduler: `./scripts/start_scheduler.sh`
2. Add a long-running task: `./scripts/add_task.sh "Long Task" HIGH 30000`
3. Kill a worker: `kill -9 <worker_pid>`
4. Check logs: Task should be recovered and retried

### Test Task Timeout

1. Start scheduler: `./scripts/start_scheduler.sh`
2. Add task with short timeout: `./scripts/add_task.sh "Test" HIGH 10000 5`
3. Wait 5+ seconds
4. Check logs: Task should timeout and be retried or marked TIMEOUT

---

## 📊 Benefits

1. **Reliability**: No task loss on worker crashes
2. **Stability**: No system hangs from stuck tasks
3. **Visibility**: Clear timeout and retry information
4. **Automatic Recovery**: System self-heals from failures
5. **Configurable**: Timeout and retry limits are configurable

---

## ⚙️ Configuration

Edit `config.h` to customize:

```c
#define DEFAULT_TASK_TIMEOUT_SECONDS 300  // Default timeout
#define MAX_TASK_RETRIES 3                 // Max retries
#define TASK_TIMEOUT_CHECK_INTERVAL 2      // Check interval (seconds)
```

---

## 🎯 Next Steps (Future Improvements)

1. **Thread Pool Management** - Track and limit active threads
2. **Task Cancellation API** - Allow canceling running tasks
3. **Load Balancing** - Distribute tasks based on worker load
4. **Persistence** - Save task state to disk for recovery

---

**All changes are backward compatible. Existing tasks without timeout will use default timeout (300s).**

