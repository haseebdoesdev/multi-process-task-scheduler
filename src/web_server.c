#include "common.h"
#include "task_queue.h"
#include "logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define MAX_REQUEST_SIZE 4096

static TaskQueue* queue = NULL;
static volatile int server_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        server_running = 0;
        LOG_INFO_F("Web server shutdown signal received");
    }
}

// Read a line from socket
int read_line(int sockfd, char* buffer, int max_len) {
    int i = 0;
    char c;
    while (i < max_len - 1) {
        if (recv(sockfd, &c, 1, 0) <= 0) {
            return -1;
        }
        if (c == '\n') {
            buffer[i] = '\0';
            return i;
        }
        if (c != '\r') {
            buffer[i++] = c;
        }
    }
    buffer[i] = '\0';
    return i;
}

// Send HTTP response
void send_response(int sockfd, int status_code, const char* content_type, const char* body, int body_len) {
    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, content_type, body_len);
    
    send(sockfd, header, strlen(header), 0);
    if (body && body_len > 0) {
        send(sockfd, body, body_len, 0);
    }
}

// Generate JSON for task status
void generate_status_json(char* buffer, int buffer_size) {
    if (queue == NULL) {
        snprintf(buffer, buffer_size, "{\"error\":\"Queue not available\"}");
        return;
    }
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    int pending = get_pending_task_count(queue);
    int running = get_running_task_count(queue);
    int completed = queue->completed_tasks;
    int failed = queue->failed_tasks;
    int total = queue->total_tasks;
    
    snprintf(buffer, buffer_size,
        "{"
        "\"total_tasks\":%d,"
        "\"completed_tasks\":%d,"
        "\"failed_tasks\":%d,"
        "\"pending_tasks\":%d,"
        "\"running_tasks\":%d,"
        "\"active_workers\":%d,"
        "\"queue_size\":%d,"
        "\"queue_capacity\":%d,"
        "\"algorithm\":\"%s\","
        "\"num_cpu_cores\":%d"
        "}",
        total, completed, failed, pending, running,
        queue->num_active_workers, queue->size, queue->capacity,
        algorithm_to_string(queue->algorithm),
        queue->num_cpu_cores);
    
    pthread_mutex_unlock(&queue->queue_mutex);
}

// Generate JSON for tasks list
void generate_tasks_json(char* buffer, int buffer_size) {
    if (queue == NULL) {
        snprintf(buffer, buffer_size, "{\"error\":\"Queue not available\"}");
        return;
    }
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    strcpy(buffer, "{\"tasks\":[");
    int buffer_used = strlen(buffer);
    int first = 1;
    
    for (int i = 0; i < queue->size; i++) {
        Task* task = &queue->tasks[i];
        
        // Check if we have enough space (reserve at least 2100 bytes per task + comma + closing)
        if (buffer_used > buffer_size - 2100) {
            break;  // Buffer getting full, stop adding tasks
        }
        
        char creation_time[64], start_time[64], end_time[64];
        format_timestamp(task->creation_time, creation_time, sizeof(creation_time));
        if (task->start_time > 0) {
            format_timestamp(task->start_time, start_time, sizeof(start_time));
        } else {
            strcpy(start_time, "");
        }
        if (task->end_time > 0) {
            format_timestamp(task->end_time, end_time, sizeof(end_time));
        } else {
            strcpy(end_time, "");
        }
        
        // Calculate progress for running tasks with millisecond precision
        double progress = 0.0;
        if (task->status == STATUS_RUNNING && task->start_time > 0) {
            // Use high-precision timing (milliseconds) for accurate progress
            struct timespec now_ts;
            clock_gettime(CLOCK_REALTIME, &now_ts);
            long long now_ms = (long long)now_ts.tv_sec * 1000LL + now_ts.tv_nsec / 1000000LL;
            
            // start_time is in seconds, convert to milliseconds
            long long start_ms = (long long)task->start_time * 1000LL;
            
            long long elapsed_ms = now_ms - start_ms;
            if (task->execution_time_ms > 0 && elapsed_ms > 0) {
                progress = ((double)elapsed_ms) / (double)task->execution_time_ms * 100.0;
                if (progress > 100.0) progress = 100.0;
            }
        } else if (task->status == STATUS_COMPLETED) {
            progress = 100.0;
        }
        
        // Add comma if not first task (with bounds check)
        if (!first) {
            int remaining = buffer_size - buffer_used - 1;
            if (remaining > 0) {
                strcat(buffer, ",");
                buffer_used++;
            } else {
                break;  // No space for comma
            }
        }
        first = 0;
        
        // Calculate deadline countdown (if applicable)
        int deadline_seconds = -1;
        if (task->deadline_time > 0) {
            time_t now = time(NULL);
            deadline_seconds = (int)(task->deadline_time - now);
            if (deadline_seconds < 0) deadline_seconds = 0;
        }
        
        // Increased buffer size to prevent overflow with long task names
        char task_json[2048];
        int written = snprintf(task_json, sizeof(task_json),
            "{"
            "\"id\":%d,"
            "\"name\":\"%s\","
            "\"priority\":\"%s\","
            "\"status\":\"%s\","
            "\"creation_time\":\"%s\","
            "\"start_time\":\"%s\","
            "\"end_time\":\"%s\","
            "\"execution_time_ms\":%u,"
            "\"worker_id\":%d,"
            "\"progress\":%.2f,"
            "\"deadline_time\":%ld,"
            "\"deadline_seconds\":%d,"
            "\"gang_id\":%d,"
            "\"cpu_time_used\":%u,"
            "\"current_mlfq_level\":\"%s\","
            "\"lottery_tickets\":%u,"
            "\"remaining_time_ms\":%u"
            "}",
            task->id, task->name,
            priority_to_string(task->priority),
            status_to_string(task->status),
            creation_time, start_time, end_time,
            task->execution_time_ms, task->worker_id, progress,
            task->deadline_time, deadline_seconds, task->gang_id,
            task->cpu_time_used,
            priority_to_string(task->current_mlfq_level),
            task->lottery_tickets,
            task->remaining_time_ms);
        
        // Check if snprintf succeeded and there's space
        if (written < 0 || written >= (int)sizeof(task_json)) {
            // Truncation occurred or error, skip this task
            continue;
        }
        
        // Safely append task_json with bounds checking
        int remaining = buffer_size - buffer_used - 1;
        if (remaining > 0) {
            int to_copy = written;
            if (to_copy >= remaining) {
                to_copy = remaining - 1;
            }
            strncat(buffer, task_json, to_copy);
            buffer[buffer_used + to_copy] = '\0';
            buffer_used += to_copy;
        } else {
            break;  // Buffer full
        }
    }
    
    // Safely append closing bracket
    int remaining = buffer_size - buffer_used - 3;  // Reserve for "]}"
    if (remaining > 0) {
        strcat(buffer, "]}");
    } else {
        // Emergency fallback: just close what we have
        buffer[buffer_size - 3] = '\0';
        strcat(buffer, "]}");
    }
    
    pthread_mutex_unlock(&queue->queue_mutex);
}

// Generate JSON for workers status
void generate_workers_json(char* buffer, int buffer_size) {
    if (queue == NULL) {
        snprintf(buffer, buffer_size, "{\"error\":\"Queue not available\"}");
        return;
    }
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    int active_workers = queue->num_active_workers;
    
    snprintf(buffer, buffer_size,
        "{"
        "\"active_workers\":%d,"
        "\"total_workers\":%d,"
        "\"scheduler_pid\":%d"
        "}",
        active_workers, NUM_WORKERS, (int)queue->scheduler_pid);
    
    pthread_mutex_unlock(&queue->queue_mutex);
}


// Serve static file
void serve_file(int sockfd, const char* filepath, const char* content_type) {
    // Prepend "web/" to filepath for web directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "web/%s", filepath);
    
    FILE* file = fopen(full_path, "r");
    if (file == NULL) {
        send_response(sockfd, 404, "text/html", "<h1>404 Not Found</h1>", 21);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(file_size + 1);
    if (content == NULL) {
        fclose(file);
        send_response(sockfd, 500, "text/html", "<h1>500 Internal Server Error</h1>", 33);
        return;
    }
    
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    
    send_response(sockfd, 200, content_type, content, file_size);
    free(content);
}

// Read HTTP body for POST requests
int read_http_body(int sockfd, char* buffer, int max_len, int content_length) {
    int total_read = 0;
    char c;
    while (total_read < content_length && total_read < max_len - 1) {
        if (recv(sockfd, &c, 1, 0) <= 0) {
            return -1;
        }
        buffer[total_read++] = c;
    }
    buffer[total_read] = '\0';
    return total_read;
}

// Parse JSON from POST body (simple parsing for our needs)
int parse_json_field(const char* json, const char* field, char* value, int max_len) {
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\"", field);
    const char* field_pos = strstr(json, search_pattern);
    if (!field_pos) return -1;
    
    const char* colon = strchr(field_pos, ':');
    if (!colon) return -1;
    
    colon++; // Skip ':'
    while (*colon == ' ' || *colon == '\t') colon++; // Skip whitespace
    
    if (*colon == '"') {
        colon++; // Skip opening quote
        int i = 0;
        while (*colon != '"' && *colon != '\0' && i < max_len - 1) {
            value[i++] = *colon++;
        }
        value[i] = '\0';
    } else {
        // Number
        int i = 0;
        while ((*colon >= '0' && *colon <= '9') && i < max_len - 1) {
            value[i++] = *colon++;
        }
        value[i] = '\0';
    }
    return 0;
}

// Handle POST request to add task
void handle_add_task_post(int sockfd, const char* body, int body_len) {
    (void)body_len;  // Suppress unused parameter warning
    if (queue == NULL) {
        send_response(sockfd, 500, "application/json", "{\"error\":\"Queue not available\"}", 33);
        return;
    }
    
    char name[256] = {0};
    char priority_str[32] = {0};
    char duration_str[32] = {0};
    char deadline_str[32] = {0};
    char gang_id_str[32] = {0};
    
    parse_json_field(body, "name", name, sizeof(name));
    parse_json_field(body, "priority", priority_str, sizeof(priority_str));
    parse_json_field(body, "duration", duration_str, sizeof(duration_str));
    parse_json_field(body, "deadline", deadline_str, sizeof(deadline_str));  // Optional: seconds from now
    parse_json_field(body, "gang_id", gang_id_str, sizeof(gang_id_str));     // Optional: gang ID
    
    if (strlen(name) == 0 || strlen(priority_str) == 0 || strlen(duration_str) == 0) {
        send_response(sockfd, 400, "application/json", "{\"error\":\"Missing required fields\"}", 36);
        return;
    }
    
    Priority priority;
    if (strcasecmp(priority_str, "HIGH") == 0) priority = PRIORITY_HIGH;
    else if (strcasecmp(priority_str, "MEDIUM") == 0) priority = PRIORITY_MEDIUM;
    else if (strcasecmp(priority_str, "LOW") == 0) priority = PRIORITY_LOW;
    else {
        send_response(sockfd, 400, "application/json", "{\"error\":\"Invalid priority\"}", 28);
        return;
    }
    
    unsigned int duration = (unsigned int)atoi(duration_str);
    if (duration == 0) {
        send_response(sockfd, 400, "application/json", "{\"error\":\"Invalid duration\"}", 29);
        return;
    }
    
    // Parse optional fields
    time_t deadline_time = 0;
    if (strlen(deadline_str) > 0) {
        int deadline_seconds = atoi(deadline_str);
        if (deadline_seconds > 0) {
            deadline_time = time(NULL) + deadline_seconds;
        }
    }
    
    int gang_id = -1;
    if (strlen(gang_id_str) > 0) {
        gang_id = atoi(gang_id_str);
        if (gang_id < 0) gang_id = -1;
    }
    
    int task_id;
    if (deadline_time > 0 || gang_id >= 0) {
        task_id = enqueue_task_advanced(queue, name, priority, duration, deadline_time, gang_id);
    } else {
        task_id = enqueue_task(queue, name, priority, duration);
    }
    
    if (task_id > 0) {
        char response[256];
        snprintf(response, sizeof(response), "{\"success\":true,\"task_id\":%d,\"message\":\"Task added successfully\"}", task_id);
        send_response(sockfd, 200, "application/json", response, strlen(response));
    } else {
        send_response(sockfd, 500, "application/json", "{\"error\":\"Failed to add task (queue might be full)\"}", 50);
    }
}

// Handle POST request to set scheduling algorithm
void handle_set_algorithm_post(int sockfd, const char* body, int body_len) {
    (void)body_len;
    if (queue == NULL) {
        send_response(sockfd, 500, "application/json", "{\"error\":\"Queue not available\"}", 33);
        return;
    }
    
    char algorithm_str[32] = {0};
    parse_json_field(body, "algorithm", algorithm_str, sizeof(algorithm_str));
    
    if (strlen(algorithm_str) == 0) {
        send_response(sockfd, 400, "application/json", "{\"error\":\"Missing algorithm field\"}", 37);
        return;
    }
    
    SchedulingAlgorithm algorithm;
    if (strcasecmp(algorithm_str, "PRIORITY") == 0) algorithm = SCHED_ALGORITHM_PRIORITY;
    else if (strcasecmp(algorithm_str, "EDF") == 0) algorithm = SCHED_ALGORITHM_EDF;
    else if (strcasecmp(algorithm_str, "MLFQ") == 0) algorithm = SCHED_ALGORITHM_MLFQ;
    else if (strcasecmp(algorithm_str, "GANG") == 0) algorithm = SCHED_ALGORITHM_GANG;
    else if (strcasecmp(algorithm_str, "ROUND_ROBIN") == 0 || strcasecmp(algorithm_str, "RR") == 0) algorithm = SCHED_ALGORITHM_ROUND_ROBIN;
    else if (strcasecmp(algorithm_str, "SJF") == 0) algorithm = SCHED_ALGORITHM_SJF;
    else if (strcasecmp(algorithm_str, "FIFO") == 0 || strcasecmp(algorithm_str, "FCFS") == 0) algorithm = SCHED_ALGORITHM_FIFO;
    else if (strcasecmp(algorithm_str, "LOTTERY") == 0) algorithm = SCHED_ALGORITHM_LOTTERY;
    else if (strcasecmp(algorithm_str, "SRTF") == 0) algorithm = SCHED_ALGORITHM_SRTF;
    else {
        send_response(sockfd, 400, "application/json", "{\"error\":\"Invalid algorithm\"}", 29);
        return;
    }
    
    if (set_scheduling_algorithm(queue, algorithm) == 0) {
        char response[128];
        snprintf(response, sizeof(response), "{\"success\":true,\"algorithm\":\"%s\",\"message\":\"Algorithm set successfully\"}", algorithm_to_string(algorithm));
        send_response(sockfd, 200, "application/json", response, strlen(response));
    } else {
        send_response(sockfd, 500, "application/json", "{\"error\":\"Failed to set algorithm\"}", 33);
    }
}

// Thread data structure for simulation
typedef struct {
    TaskQueue* queue;
    char scenario[64];
    int task_count;
    int interval_ms;
} SimulationData;

// Run simulation in a separate thread (non-blocking)
void* run_simulation_thread(void* arg) {
    SimulationData* data = (SimulationData*)arg;
    TaskQueue* queue = data->queue;
    const char* scenario = data->scenario;
    int task_count = data->task_count;
    int interval_ms = data->interval_ms;
    
    // Generate tasks based on scenario
    int added = 0;
    
    for (int i = 0; i < task_count; i++) {
        char task_name[128];
        Priority priority;
        unsigned int duration;
        
        if (strcmp(scenario, "priority") == 0) {
            // Priority test: Add LOW first, then MEDIUM, then HIGH to demonstrate priority ordering
            // LOW tasks added first should execute after HIGH tasks
            if (i < task_count / 3) {
                priority = PRIORITY_LOW;
                snprintf(task_name, sizeof(task_name), "Low Priority Task %d", i+1);
                duration = 6000;
            } else if (i < (task_count * 2) / 3) {
                priority = PRIORITY_MEDIUM;
                snprintf(task_name, sizeof(task_name), "Medium Priority Task %d", i - (task_count/3) + 1);
                duration = 5000;
            } else {
                priority = PRIORITY_HIGH;
                snprintf(task_name, sizeof(task_name), "High Priority Task %d", i - (task_count*2/3) + 1);
                duration = 3000;
            }
        } else if (strcmp(scenario, "burst") == 0) {
            // Burst load: all tasks at once
            priority = (i % 3 == 0) ? PRIORITY_HIGH : ((i % 3 == 1) ? PRIORITY_MEDIUM : PRIORITY_LOW);
            snprintf(task_name, sizeof(task_name), "Burst Task %d", i+1);
            duration = 2000 + (i * 200);
        } else if (strcmp(scenario, "concurrent") == 0) {
            // Concurrent execution: all HIGH priority
            priority = PRIORITY_HIGH;
            snprintf(task_name, sizeof(task_name), "Concurrent Task %d", i+1);
            duration = 5000;
        } else if (strcmp(scenario, "mixed") == 0) {
            // Mixed workload: realistic distribution
            const char* names[] = {"Data Processing", "Backup Job", "Report Gen", "Email Alert", 
                                   "Log Cleanup", "System Check", "Cache Update", "Analytics"};
            priority = (i % 4 == 0) ? PRIORITY_HIGH : ((i % 4 == 1) ? PRIORITY_MEDIUM : PRIORITY_LOW);
            snprintf(task_name, sizeof(task_name), "%s %d", names[i % 8], (i/8)+1);
            duration = 2000 + (i * 300);
        } else if (strcmp(scenario, "long-running") == 0) {
            // Long-running tasks: mix of quick and long tasks
            if (i % 3 == 0) {
                priority = PRIORITY_HIGH;
                snprintf(task_name, sizeof(task_name), "Quick Task %d", (i/3)+1);
                duration = 1000; // Very quick
            } else if (i % 3 == 1) {
                priority = PRIORITY_MEDIUM;
                snprintf(task_name, sizeof(task_name), "Long Running Job %d", (i/3)+1);
                duration = 15000; // Long running
            } else {
                priority = PRIORITY_HIGH;
                snprintf(task_name, sizeof(task_name), "Quick Task %d", (i/3)+2);
                duration = 1000; // Quick again
            }
        } else if (strcmp(scenario, "continuous") == 0) {
            // Continuous load: steady-state operation with regular intervals
            priority = (Priority)(i % 3); // Cycle through priorities
            snprintf(task_name, sizeof(task_name), "Continuous Task %d", i+1);
            duration = 3000 + (i * 1000);
        } else {
            // Default: random mix
            priority = (Priority)(i % 3);
            snprintf(task_name, sizeof(task_name), "Task %d", i+1);
            duration = 2000 + (i * 200);
        }
        
        if (enqueue_task(queue, task_name, priority, duration) > 0) {
            added++;
            if (interval_ms > 0 && i < task_count - 1) {
                usleep(interval_ms * 1000); // Convert ms to microseconds
            }
        }
    }
    
    free(data);
    return NULL;
}

// Handle POST request to run simulation
void handle_simulation_post(int sockfd, const char* body, int body_len) {
    (void)body_len;  // Suppress unused parameter warning
    if (queue == NULL) {
        send_response(sockfd, 500, "application/json", "{\"error\":\"Queue not available\"}", 33);
        return;
    }
    
    // Parse simulation parameters
    char scenario[64] = {0};
    char count_str[32] = {0};
    char interval_str[32] = {0};
    
    parse_json_field(body, "scenario", scenario, sizeof(scenario));
    parse_json_field(body, "count", count_str, sizeof(count_str));
    parse_json_field(body, "interval", interval_str, sizeof(interval_str));
    
    int task_count = count_str[0] ? atoi(count_str) : 10;
    int interval_ms = interval_str[0] ? atoi(interval_str) : 500;
    
    // Allocate thread data
    SimulationData* data = (SimulationData*)malloc(sizeof(SimulationData));
    if (data == NULL) {
        send_response(sockfd, 500, "application/json", "{\"error\":\"Memory allocation failed\"}", 37);
        return;
    }
    
    data->queue = queue;
    strncpy(data->scenario, scenario, sizeof(data->scenario) - 1);
    data->scenario[sizeof(data->scenario) - 1] = '\0';
    data->task_count = task_count;
    data->interval_ms = interval_ms;
    
    // Spawn thread to run simulation (non-blocking)
    pthread_t thread;
    if (pthread_create(&thread, NULL, run_simulation_thread, data) != 0) {
        free(data);
        send_response(sockfd, 500, "application/json", "{\"error\":\"Failed to start simulation thread\"}", 45);
        return;
    }
    
    // Detach thread so it cleans up automatically
    pthread_detach(thread);
    
    // Return immediately - simulation runs in background
    char response[256];
    snprintf(response, sizeof(response), "{\"success\":true,\"total\":%d,\"message\":\"Simulation started in background\"}", 
             task_count);
    send_response(sockfd, 200, "application/json", response, strlen(response));
}

// Handle POST request to cancel task
void handle_cancel_task_post(int sockfd, const char* body, int body_len) {
    (void)body_len;
    if (queue == NULL) {
        send_response(sockfd, 500, "application/json", "{\"error\":\"Queue not available\"}", 33);
        return;
    }
    
    char task_id_str[32] = {0};
    parse_json_field(body, "task_id", task_id_str, sizeof(task_id_str));
    
    if (strlen(task_id_str) == 0) {
        send_response(sockfd, 400, "application/json", "{\"error\":\"Missing task_id\"}", 27);
        return;
    }
    
    int task_id = atoi(task_id_str);
    int result = cancel_task(queue, task_id);
    
    if (result == 0) {
        char response[128];
        snprintf(response, sizeof(response), "{\"success\":true,\"task_id\":%d,\"message\":\"Task cancelled\"}", task_id);
        send_response(sockfd, 200, "application/json", response, strlen(response));
    } else if (result == -1) {
        send_response(sockfd, 404, "application/json", "{\"error\":\"Task not found\"}", 26);
    } else if (result == -2) {
        send_response(sockfd, 400, "application/json", "{\"error\":\"Only PENDING tasks can be cancelled\"}", 47);
    } else {
        send_response(sockfd, 500, "application/json", "{\"error\":\"Failed to cancel task\"}", 34);
    }
}

// Generate JSON for worker utilization stats
void generate_worker_stats_json(char* buffer, int buffer_size) {
    if (queue == NULL) {
        snprintf(buffer, buffer_size, "{\"error\":\"Queue not available\"}");
        return;
    }
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    // Count tasks per worker
    int worker_completed[NUM_WORKERS] = {0};
    int worker_running[NUM_WORKERS] = {0};
    int worker_total[NUM_WORKERS] = {0};
    
    for (int i = 0; i < queue->size; i++) {
        Task* task = &queue->tasks[i];
        if (task->worker_id >= 0 && task->worker_id < NUM_WORKERS) {
            worker_total[task->worker_id]++;
            if (task->status == STATUS_COMPLETED) {
                worker_completed[task->worker_id]++;
            } else if (task->status == STATUS_RUNNING) {
                worker_running[task->worker_id]++;
            }
        }
    }
    
    int active_workers = queue->num_active_workers;
    
    pthread_mutex_unlock(&queue->queue_mutex);
    
    // Build JSON
    strcpy(buffer, "{\"workers\":[");
    for (int i = 0; i < NUM_WORKERS; i++) {
        char worker_json[128];
        snprintf(worker_json, sizeof(worker_json),
            "%s{\"id\":%d,\"active\":%s,\"completed\":%d,\"running\":%d,\"total\":%d}",
            (i > 0) ? "," : "",
            i,
            (i < active_workers) ? "true" : "false",
            worker_completed[i],
            worker_running[i],
            worker_total[i]);
        strcat(buffer, worker_json);
    }
    strcat(buffer, "]}");
}

// Generate CSV export of all tasks
void generate_tasks_csv(char* buffer, int buffer_size) {
    if (queue == NULL) {
        snprintf(buffer, buffer_size, "error,Queue not available\n");
        return;
    }
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    // CSV header
    int offset = snprintf(buffer, buffer_size,
        "ID,Name,Priority,Status,Duration_ms,Worker_ID,Created,Started,Ended\n");
    
    for (int i = 0; i < queue->size && offset < buffer_size - 256; i++) {
        Task* task = &queue->tasks[i];
        
        char creation_time[64] = "", start_time[64] = "", end_time[64] = "";
        format_timestamp(task->creation_time, creation_time, sizeof(creation_time));
        if (task->start_time > 0) {
            format_timestamp(task->start_time, start_time, sizeof(start_time));
        }
        if (task->end_time > 0) {
            format_timestamp(task->end_time, end_time, sizeof(end_time));
        }
        
        offset += snprintf(buffer + offset, buffer_size - offset,
            "%d,\"%s\",%s,%s,%u,%d,%s,%s,%s\n",
            task->id, task->name,
            priority_to_string(task->priority),
            status_to_string(task->status),
            task->execution_time_ms,
            task->worker_id,
            creation_time, start_time, end_time);
    }
    
    pthread_mutex_unlock(&queue->queue_mutex);
}

// Handle API requests
void handle_api_request(int sockfd, const char* path, const char* method, const char* body, int body_len) {
    char json_buffer[16384];  // Increased for CSV export
    
    if (strcmp(path, "/api/status") == 0 && strcmp(method, "GET") == 0) {
        generate_status_json(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "application/json", json_buffer, strlen(json_buffer));
    } else if (strcmp(path, "/api/tasks") == 0 && strcmp(method, "GET") == 0) {
        generate_tasks_json(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "application/json", json_buffer, strlen(json_buffer));
    } else if (strcmp(path, "/api/workers") == 0 && strcmp(method, "GET") == 0) {
        generate_workers_json(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "application/json", json_buffer, strlen(json_buffer));
    } else if (strcmp(path, "/api/worker_stats") == 0 && strcmp(method, "GET") == 0) {
        generate_worker_stats_json(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "application/json", json_buffer, strlen(json_buffer));
    } else if (strcmp(path, "/api/export/csv") == 0 && strcmp(method, "GET") == 0) {
        generate_tasks_csv(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "text/csv", json_buffer, strlen(json_buffer));
    } else if (strcmp(path, "/api/export/json") == 0 && strcmp(method, "GET") == 0) {
        generate_tasks_json(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "application/json", json_buffer, strlen(json_buffer));
    } else if (strcmp(path, "/api/add_task") == 0 && strcmp(method, "POST") == 0) {
        handle_add_task_post(sockfd, body, body_len);
    } else if (strcmp(path, "/api/simulate") == 0 && strcmp(method, "POST") == 0) {
        handle_simulation_post(sockfd, body, body_len);
    } else if (strcmp(path, "/api/cancel_task") == 0 && strcmp(method, "POST") == 0) {
        handle_cancel_task_post(sockfd, body, body_len);
    } else if (strcmp(path, "/api/set_algorithm") == 0 && strcmp(method, "POST") == 0) {
        handle_set_algorithm_post(sockfd, body, body_len);
    } else if (strcmp(path, "/api/algorithm") == 0 && strcmp(method, "GET") == 0) {
        // Get current algorithm
        if (queue != NULL) {
            SchedulingAlgorithm alg = get_scheduling_algorithm(queue);
            char response[128];
            snprintf(response, sizeof(response), "{\"algorithm\":\"%s\"}", algorithm_to_string(alg));
            send_response(sockfd, 200, "application/json", response, strlen(response));
        } else {
            send_response(sockfd, 500, "application/json", "{\"error\":\"Queue not available\"}", 33);
        }
    } else {
        send_response(sockfd, 404, "application/json", "{\"error\":\"Not found\"}", 19);
    }
}

// Handle HTTP request
void handle_request(int sockfd) {
    char request[MAX_REQUEST_SIZE];
    char method[16], path[256], protocol[16];
    
    // Read request line
    if (read_line(sockfd, request, sizeof(request)) < 0) {
        close(sockfd);
        return;
    }
    
    sscanf(request, "%s %s %s", method, path, protocol);
    
    // Read headers and find Content-Length for POST requests
    int content_length = 0;
    char line[256];
    char body[MAX_REQUEST_SIZE] = {0};
    
    do {
        if (read_line(sockfd, line, sizeof(line)) < 0) {
            close(sockfd);
            return;
        }
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            content_length = atoi(line + 15);
        }
    } while (strlen(line) > 0);
    
    // Read body for POST requests
    if (strcmp(method, "POST") == 0 && content_length > 0) {
        read_http_body(sockfd, body, sizeof(body), content_length);
    }
    
    // Handle API requests
    if (strncmp(path, "/api/", 5) == 0) {
        handle_api_request(sockfd, path, method, body, content_length);
    }
    // Handle static files
    else if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        serve_file(sockfd, "index.html", "text/html");
    } else if (strcmp(path, "/dashboard.js") == 0) {
        serve_file(sockfd, "dashboard.js", "application/javascript");
    } else if (strcmp(path, "/dashboard.css") == 0) {
        serve_file(sockfd, "dashboard.css", "text/css");
    } else {
        send_response(sockfd, 404, "text/html", "<h1>404 Not Found</h1>", 21);
    }
    
    close(sockfd);
}

int main(int argc, char* argv[]) {
    (void)argc;  // Suppress unused parameter warning
    (void)argv;
    // Initialize logger
    init_logger("web_server");
    LOG_INFO_F("Starting web server...");
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Attach to shared memory
    queue = attach_shared_memory(-1);
    if (queue == NULL) {
        LOG_ERROR_F("Failed to attach to shared memory");
        return 1;
    }
    
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOG_ERROR_F("Failed to create socket");
        return 1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR_F("Failed to bind socket to port %d", PORT);
        close(server_fd);
        return 1;
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        LOG_ERROR_F("Failed to listen on socket");
        close(server_fd);
        return 1;
    }
    
    LOG_INFO_F("Web server listening on http://localhost:%d", PORT);
    LOG_INFO_F("Dashboard available at http://localhost:%d", PORT);
    
    // Accept connections
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (server_running) {
                LOG_ERROR_F("Failed to accept connection");
            }
            continue;
        }
        
        // Handle request (for simplicity, handle synchronously)
        // In production, would fork or use threads
        handle_request(client_fd);
    }
    
    LOG_INFO_F("Web server shutting down...");
    close(server_fd);
    detach_shared_memory(queue);
    close_logger();
    
    return 0;
}

