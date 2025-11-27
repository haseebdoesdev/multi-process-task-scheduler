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
    
    // Count timeout tasks separately
    int timeout = 0;
    for (int i = 0; i < queue->size; i++) {
        if (queue->tasks[i].status == STATUS_TIMEOUT) {
            timeout++;
        }
    }
    
    snprintf(buffer, buffer_size,
        "{"
        "\"total_tasks\":%d,"
        "\"completed_tasks\":%d,"
        "\"failed_tasks\":%d,"
        "\"timeout_tasks\":%d,"
        "\"pending_tasks\":%d,"
        "\"running_tasks\":%d,"
        "\"active_workers\":%d,"
        "\"queue_size\":%d,"
        "\"queue_capacity\":%d"
        "}",
        total, completed, failed, timeout, pending, running,
        queue->num_active_workers, queue->size, queue->capacity);
    
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
    int first = 1;
    
    for (int i = 0; i < queue->size; i++) {
        Task* task = &queue->tasks[i];
        
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
        
        // Calculate progress for running tasks
        double progress = 0.0;
        if (task->status == STATUS_RUNNING && task->start_time > 0) {
            time_t now = time(NULL);
            time_t elapsed = now - task->start_time;
            if (task->execution_time_ms > 0) {
                progress = ((double)elapsed * 1000.0) / (double)task->execution_time_ms;
                if (progress > 100.0) progress = 100.0;
            }
        } else if (task->status == STATUS_COMPLETED) {
            progress = 100.0;
        }
        
        if (!first) strcat(buffer, ",");
        first = 0;
        
        char task_json[768];
        snprintf(task_json, sizeof(task_json),
            "{"
            "\"id\":%d,"
            "\"name\":\"%s\","
            "\"priority\":\"%s\","
            "\"status\":\"%s\","
            "\"creation_time\":\"%s\","
            "\"start_time\":\"%s\","
            "\"end_time\":\"%s\","
            "\"execution_time_ms\":%u,"
            "\"timeout_seconds\":%u,"
            "\"retry_count\":%d,"
            "\"worker_id\":%d,"
            "\"progress\":%.2f"
            "}",
            task->id, task->name,
            priority_to_string(task->priority),
            status_to_string(task->status),
            creation_time, start_time, end_time,
            task->execution_time_ms, task->timeout_seconds, task->retry_count,
            task->worker_id, progress);
        
        strcat(buffer, task_json);
    }
    
    strcat(buffer, "]}");
    
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

// Handle API requests
void handle_api_request(int sockfd, const char* path) {
    char json_buffer[8192];
    
    if (strcmp(path, "/api/status") == 0) {
        generate_status_json(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "application/json", json_buffer, strlen(json_buffer));
    } else if (strcmp(path, "/api/tasks") == 0) {
        generate_tasks_json(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "application/json", json_buffer, strlen(json_buffer));
    } else if (strcmp(path, "/api/workers") == 0) {
        generate_workers_json(json_buffer, sizeof(json_buffer));
        send_response(sockfd, 200, "application/json", json_buffer, strlen(json_buffer));
    } else {
        send_response(sockfd, 404, "application/json", "{\"error\":\"Not found\"}", 19);
    }
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
    
    // Skip headers
    char line[256];
    do {
        if (read_line(sockfd, line, sizeof(line)) < 0) {
            close(sockfd);
            return;
        }
    } while (strlen(line) > 0);
    
    // Handle API requests
    if (strncmp(path, "/api/", 5) == 0) {
        handle_api_request(sockfd, path);
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

