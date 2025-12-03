#include "common.h"

const char* priority_to_string(Priority p) {
    switch (p) {
        case PRIORITY_HIGH:   return "HIGH";
        case PRIORITY_MEDIUM: return "MEDIUM";
        case PRIORITY_LOW:    return "LOW";
        default:              return "UNKNOWN";
    }
}

const char* status_to_string(TaskStatus s) {
    switch (s) {
        case STATUS_PENDING:  return "PENDING";
        case STATUS_RUNNING:  return "RUNNING";
        case STATUS_COMPLETED: return "COMPLETED";
        case STATUS_FAILED:   return "FAILED";
        default:              return "UNKNOWN";
    }
}

const char* algorithm_to_string(SchedulingAlgorithm alg) {
    switch (alg) {
        case SCHED_ALGORITHM_PRIORITY: return "PRIORITY";
        case SCHED_ALGORITHM_EDF:      return "EDF";
        case SCHED_ALGORITHM_MLFQ:     return "MLFQ";
        case SCHED_ALGORITHM_GANG:     return "GANG";
        case SCHED_ALGORITHM_ROUND_ROBIN: return "ROUND_ROBIN";
        case SCHED_ALGORITHM_SJF:      return "SJF";
        case SCHED_ALGORITHM_FIFO:     return "FIFO";
        case SCHED_ALGORITHM_LOTTERY:  return "LOTTERY";
        case SCHED_ALGORITHM_SRTF:     return "SRTF";
        default:                       return "UNKNOWN";
    }
}

time_t get_current_time(void) {
    return time(NULL);
}

void format_timestamp(time_t t, char* buffer, size_t size) {
    struct tm* tm_info = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

