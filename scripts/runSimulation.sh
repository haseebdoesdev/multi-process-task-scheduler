#!/bin/bash

# Simulation Script
# Demonstrates various mechanisms of the task scheduler system
# Shows priority-based scheduling, concurrent execution, worker distribution, etc.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  🎯 Task Scheduler Simulation${NC}"
echo -e "${CYAN}  Demonstrating System Mechanisms${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Check if scheduler is running
if [ ! -f scheduler.pid ]; then
    echo -e "${RED}Error: Scheduler is not running!${NC}"
    echo "Please start the scheduler first:"
    echo "  ./scripts/start_scheduler.sh"
    exit 1
fi

PID=$(cat scheduler.pid)
if ! ps -p "$PID" > /dev/null 2>&1; then
    echo -e "${RED}Error: Scheduler is not running (stale PID file)${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Scheduler is running (PID: $PID)${NC}"
echo ""

# Function to add task with colored output
add_task() {
    local name=$1
    local priority=$2
    local duration=$3
    local description=$4
    
    local color
    case $priority in
        HIGH) color=$RED ;;
        MEDIUM) color=$YELLOW ;;
        LOW) color=$BLUE ;;
        *) color=$NC ;;
    esac
    
    echo -e "${color}Adding task: $name${NC}"
    echo -e "  Priority: ${color}$priority${NC} | Duration: ${duration}ms | $description"
    
    ./scripts/add_task.sh "$name" "$priority" "$duration" > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}✓ Task added successfully${NC}"
    else
        echo -e "  ${RED}✗ Failed to add task${NC}"
    fi
    echo ""
}

# Function to wait with message
wait_interval() {
    local seconds=$1
    local message=$2
    echo -e "${MAGENTA}⏳ $message (waiting ${seconds}s...)${NC}"
    sleep $seconds
    echo ""
}

echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}Phase 1: Priority-Based Scheduling Demonstration${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "This demonstrates that HIGH priority tasks execute before LOW priority tasks,"
echo "even if LOW priority tasks were added first."
echo ""

# Add tasks in reverse priority order
add_task "Low Priority Task 1" LOW 8000 "Added first, but should execute last"
add_task "Low Priority Task 2" LOW 6000 "Also low priority"
add_task "Medium Priority Task 1" MEDIUM 5000 "Medium priority"
add_task "High Priority Task 1" HIGH 3000 "Added last, but should execute first!"
add_task "High Priority Task 2" HIGH 4000 "Another high priority task"

echo -e "${YELLOW}→ Watch the dashboard or monitor - HIGH priority tasks should start first!${NC}"
wait_interval 8 "Observing priority scheduling in action"

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}Phase 2: Concurrent Execution Demonstration${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "This demonstrates that multiple tasks can run concurrently"
echo "using different worker processes."
echo ""

# Add multiple tasks simultaneously
echo -e "${GREEN}Adding multiple tasks at once to test concurrent execution...${NC}"
echo ""

add_task "Concurrent Task 1" HIGH 6000 "Should run on Worker 0"
add_task "Concurrent Task 2" HIGH 6000 "Should run on Worker 1"
add_task "Concurrent Task 3" HIGH 6000 "Should run on Worker 2"
add_task "Concurrent Task 4" MEDIUM 5000 "Should queue and run when worker available"

echo -e "${YELLOW}→ Multiple tasks should run simultaneously across different workers!${NC}"
wait_interval 7 "Observing concurrent execution"

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}Phase 3: Queue Management Demonstration${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "This demonstrates task queuing and execution as workers become available."
echo ""

# Fill the queue with tasks
echo -e "${GREEN}Adding a burst of tasks to test queue management...${NC}"
echo ""

for i in {1..10}; do
    priority="MEDIUM"
    if [ $((i % 3)) -eq 0 ]; then
        priority="HIGH"
    elif [ $((i % 5)) -eq 0 ]; then
        priority="LOW"
    fi
    
    duration=$((2000 + (i * 500)))
    add_task "Queue Task $i" "$priority" "$duration" "Queue management test"
    sleep 0.3  # Small delay between additions
done

echo -e "${YELLOW}→ Tasks should queue and execute as workers become available!${NC}"
wait_interval 10 "Observing queue management"

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}Phase 4: Mixed Workload Demonstration${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "This demonstrates a realistic mixed workload with various task types."
echo ""

add_task "Data Processing Job" HIGH 4000 "Critical data processing"
add_task "Backup Operation" MEDIUM 8000 "Scheduled backup"
add_task "Report Generation" LOW 3000 "Low priority report"
add_task "Email Notification" HIGH 1000 "Urgent notification"
add_task "Log Cleanup" LOW 2000 "Maintenance task"
add_task "System Check" MEDIUM 2500 "Regular check"
add_task "Cache Update" HIGH 1500 "Important update"
add_task "Analytics Job" LOW 6000 "Non-urgent analytics"

echo -e "${YELLOW}→ Observe how different priority tasks are scheduled!${NC}"
wait_interval 10 "Observing mixed workload execution"

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}Phase 5: Burst Load Test${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "This demonstrates system behavior under burst load conditions."
echo ""

echo -e "${GREEN}Creating burst load with 15 tasks in quick succession...${NC}"
echo ""

burst_count=1
for priority in HIGH MEDIUM HIGH LOW MEDIUM HIGH LOW MEDIUM LOW HIGH MEDIUM LOW MEDIUM LOW; do
    duration=$((1000 + (burst_count * 300)))
    add_task "Burst Task $burst_count" "$priority" "$duration" "Burst load test"
    sleep 0.2
    burst_count=$((burst_count + 1))
done

echo -e "${YELLOW}→ System should handle burst load gracefully!${NC}"
wait_interval 15 "Observing burst load handling"

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}Phase 6: Long Running Tasks${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "This demonstrates handling of long-running tasks alongside short tasks."
echo ""

add_task "Quick Task 1" HIGH 1000 "Very quick task"
add_task "Long Running Job" MEDIUM 15000 "Takes 15 seconds"
add_task "Quick Task 2" HIGH 1000 "Another quick task"
add_task "Quick Task 3" HIGH 1000 "Third quick task"
add_task "Medium Job" LOW 8000 "Medium duration job"

echo -e "${YELLOW}→ Quick tasks should complete while long task is still running!${NC}"
wait_interval 12 "Observing mixed duration tasks"

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}Final Phase: Continuous Load${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "Adding final set of tasks at regular intervals to show steady-state operation."
echo ""

for i in {1..5}; do
    priority_index=$((i % 3))
    case $priority_index in
        0) priority="HIGH" ;;
        1) priority="MEDIUM" ;;
        2) priority="LOW" ;;
    esac
    
    duration=$((3000 + (i * 1000)))
    add_task "Final Task $i" "$priority" "$duration" "Final phase task"
    
    if [ $i -lt 5 ]; then
        wait_interval 2 "Waiting before next task"
    fi
done

echo ""
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}  ✅ Simulation Complete!${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${CYAN}Summary of demonstrated mechanisms:${NC}"
echo ""
echo -e "  ✓ ${GREEN}Priority-based scheduling${NC} - High priority tasks execute first"
echo -e "  ✓ ${GREEN}Concurrent execution${NC} - Multiple tasks run in parallel"
echo -e "  ✓ ${GREEN}Queue management${NC} - Tasks queue when workers are busy"
echo -e "  ✓ ${GREEN}Worker distribution${NC} - Tasks distributed across workers"
echo -e "  ✓ ${GREEN}Burst load handling${NC} - System handles sudden load spikes"
echo -e "  ✓ ${GREEN}Mixed workload${NC} - Different task types and durations"
echo ""
echo -e "${YELLOW}💡 Tip: Open the web dashboard to see visual updates:${NC}"
echo -e "   http://localhost:8080"
echo ""
echo -e "${YELLOW}Or monitor in terminal:${NC}"
echo -e "   ./scripts/monitor.sh"
echo ""
echo -e "${CYAN}Tasks are still running in the background.${NC}"
echo -e "Check the dashboard or monitor to see completion status."
echo ""


