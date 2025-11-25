# Makefile for Multi-Process Task Scheduler

CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11 -pthread -D_GNU_SOURCE
LDFLAGS = -lpthread -lrt

# Directories
SRC_DIR = src
BUILD_DIR = build
SCRIPTS_DIR = scripts

# Source files
COMMON_SRC = $(SRC_DIR)/common.c
TASK_QUEUE_SRC = $(SRC_DIR)/task_queue.c
LOGGER_SRC = $(SRC_DIR)/logger.c
SCHEDULER_SRC = $(SRC_DIR)/scheduler.c
WORKER_SRC = $(SRC_DIR)/worker.c

# Object files
COMMON_OBJ = $(BUILD_DIR)/common.o
TASK_QUEUE_OBJ = $(BUILD_DIR)/task_queue.o
LOGGER_OBJ = $(BUILD_DIR)/logger.o
SCHEDULER_OBJ = $(BUILD_DIR)/scheduler.o
WORKER_OBJ = $(BUILD_DIR)/worker.o

# Executables
SCHEDULER = scheduler
WORKER = worker

# Header files
HEADERS = config.h $(SRC_DIR)/common.h $(SRC_DIR)/task_queue.h $(SRC_DIR)/logger.h

# Default target
all: $(SCHEDULER) $(WORKER) scripts

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Common object file
$(COMMON_OBJ): $(SRC_DIR)/common.c $(SRC_DIR)/common.h config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Task queue object file
$(TASK_QUEUE_OBJ): $(SRC_DIR)/task_queue.c $(SRC_DIR)/task_queue.h $(SRC_DIR)/common.h config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Logger object file
$(LOGGER_OBJ): $(SRC_DIR)/logger.c $(SRC_DIR)/logger.h $(SRC_DIR)/common.h config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Scheduler executable
$(SCHEDULER): $(SCHEDULER_OBJ) $(TASK_QUEUE_OBJ) $(COMMON_OBJ) $(LOGGER_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Scheduler object file
$(SCHEDULER_OBJ): $(SRC_DIR)/scheduler.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Worker executable
$(WORKER): $(WORKER_OBJ) $(TASK_QUEUE_OBJ) $(COMMON_OBJ) $(LOGGER_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Worker object file
$(WORKER_OBJ): $(SRC_DIR)/worker.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Make scripts executable
scripts:
	chmod +x $(SCRIPTS_DIR)/*.sh

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(SCHEDULER) $(WORKER)
	rm -f add_task_helper monitor_helper report_helper
	rm -f *.c # Remove any generated .c files from scripts

# Clean everything including logs and shared memory
distclean: clean
	rm -rf logs
	rm -f scheduler.pid
	rm -f *.csv
	$(SCRIPTS_DIR)/cleanup.sh

# Install (optional - just makes scripts available)
install: all
	@echo "Build complete. Use ./scripts/start_scheduler.sh to start"

# Debug build
debug: CFLAGS += -DDEBUG -g3
debug: all

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: clean all

.PHONY: all clean distclean install debug release scripts

