// Dashboard JavaScript for real-time updates

const API_BASE = '';
const REFRESH_INTERVAL = 2000; // 2 seconds
let autoRefresh = true;
let refreshIntervalId = null;
let throughputData = [];
let maxDataPoints = 30;

// Chart instances
let throughputChart = null;
let statusChart = null;
let workerChart = null;

// Current tasks data for modal
let currentTasks = [];

// Initialize dashboard
document.addEventListener('DOMContentLoaded', () => {
    initializeCharts();
    setupEventListeners();
    setupForms();
    startAutoRefresh();
    updateDashboard();
});

// Initialize Chart.js charts
function initializeCharts() {
    // Throughput Chart
    const throughputCtx = document.getElementById('throughputChart').getContext('2d');
    throughputChart = new Chart(throughputCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Tasks Completed',
                data: [],
                borderColor: 'rgb(74, 144, 226)',
                backgroundColor: 'rgba(74, 144, 226, 0.1)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: true,
                    labels: {
                        color: '#f1f5f9'
                    }
                }
            },
            scales: {
                x: {
                    ticks: { color: '#94a3b8' },
                    grid: { color: '#334155' }
                },
                y: {
                    beginAtZero: true,
                    ticks: { color: '#94a3b8' },
                    grid: { color: '#334155' }
                }
            }
        }
    });

    // Status Distribution Chart
    const statusCtx = document.getElementById('statusChart').getContext('2d');
    statusChart = new Chart(statusCtx, {
        type: 'doughnut',
        data: {
            labels: ['Pending', 'Running', 'Completed', 'Failed'],
            datasets: [{
                data: [0, 0, 0, 0],
                backgroundColor: [
                    'rgba(52, 152, 219, 0.8)',
                    'rgba(243, 156, 18, 0.8)',
                    'rgba(46, 204, 113, 0.8)',
                    'rgba(231, 76, 60, 0.8)'
                ],
                borderColor: [
                    'rgb(52, 152, 219)',
                    'rgb(243, 156, 18)',
                    'rgb(46, 204, 113)',
                    'rgb(231, 76, 60)'
                ],
                borderWidth: 2
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    position: 'bottom',
                    labels: {
                        color: '#f1f5f9',
                        padding: 15
                    }
                }
            }
        }
    });
    
    // Worker Utilization Chart
    const workerCtx = document.getElementById('workerChart').getContext('2d');
    workerChart = new Chart(workerCtx, {
        type: 'bar',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Running',
                    data: [],
                    backgroundColor: 'rgb(243, 156, 18)',
                    stack: 'Stack 0'
                },
                {
                    label: 'Completed',
                    data: [],
                    backgroundColor: 'rgb(46, 204, 113)',
                    stack: 'Stack 0'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: true,
                    labels: {
                        color: '#f1f5f9'
                    }
                }
            },
            scales: {
                x: {
                    stacked: true,
                    ticks: { color: '#94a3b8' },
                    grid: { color: '#334155' }
                },
                y: {
                    stacked: true,
                    beginAtZero: true,
                    ticks: { color: '#94a3b8' },
                    grid: { color: '#334155' }
                }
            }
        }
    });
}

// Setup event listeners
function setupEventListeners() {
    document.getElementById('refreshBtn').addEventListener('click', () => {
        updateDashboard();
    });

    document.getElementById('autoRefreshToggle').addEventListener('click', () => {
        autoRefresh = !autoRefresh;
        if (autoRefresh) {
            startAutoRefresh();
            document.getElementById('autoRefreshToggle').textContent = '⏸️ Auto: ON';
        } else {
            stopAutoRefresh();
            document.getElementById('autoRefreshToggle').textContent = '▶️ Auto: OFF';
        }
    });

    document.getElementById('statusFilter').addEventListener('change', () => updateTaskTable(currentTasks));
    document.getElementById('priorityFilter').addEventListener('change', () => updateTaskTable(currentTasks));
    
    // Export buttons
    document.getElementById('exportCsvBtn').addEventListener('click', exportCSV);
    document.getElementById('exportJsonBtn').addEventListener('click', exportJSON);
    
    // Modal close
    document.getElementById('modalClose').addEventListener('click', closeModal);
    document.getElementById('taskModal').addEventListener('click', (e) => {
        if (e.target.id === 'taskModal') closeModal();
    });
}

function setupForms() {
    // Add Task Form
    const addTaskForm = document.getElementById('addTaskForm');
    const addTaskMessage = document.getElementById('addTaskMessage');
    
    addTaskForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        addTaskMessage.className = 'message';
        addTaskMessage.textContent = 'Adding task...';
        addTaskMessage.className = 'message info';
        
        const formData = {
            name: document.getElementById('taskName').value,
            priority: document.getElementById('taskPriority').value,
            duration: parseInt(document.getElementById('taskDuration').value)
        };
        
        try {
            const response = await fetch('/api/add_task', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(formData)
            });
            
            const result = await response.json();
            
            if (result.success) {
                addTaskMessage.textContent = `✅ ${result.message} (Task ID: ${result.task_id})`;
                addTaskMessage.className = 'message success';
                addTaskForm.reset();
                // Refresh dashboard after a short delay
                setTimeout(() => updateDashboard(), 500);
            } else {
                addTaskMessage.textContent = `❌ Error: ${result.error || 'Failed to add task'}`;
                addTaskMessage.className = 'message error';
            }
        } catch (error) {
            addTaskMessage.textContent = `❌ Network error: ${error.message}`;
            addTaskMessage.className = 'message error';
        }
        
        // Clear message after 5 seconds
        setTimeout(() => {
            addTaskMessage.className = 'message';
            addTaskMessage.textContent = '';
        }, 5000);
    });
    
    // Simulation Form
    const simulationForm = document.getElementById('simulationForm');
    const simulationMessage = document.getElementById('simulationMessage');
    
    simulationForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        simulationMessage.className = 'message';
        simulationMessage.textContent = 'Starting simulation...';
        simulationMessage.className = 'message info';
        
        const formData = {
            scenario: document.getElementById('simScenario').value,
            count: parseInt(document.getElementById('simCount').value),
            interval: parseInt(document.getElementById('simInterval').value)
        };
        
        try {
            const response = await fetch('/api/simulate', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(formData)
            });
            
            const result = await response.json();
            
            if (result.success) {
                simulationMessage.textContent = `✅ ${result.message || 'Simulation started in background'} (${result.total} tasks)`;
                simulationMessage.className = 'message success';
                // Refresh dashboard after a short delay to see tasks appear
                setTimeout(() => updateDashboard(), 500);
            } else {
                simulationMessage.textContent = `❌ Error: ${result.error || 'Failed to start simulation'}`;
                simulationMessage.className = 'message error';
            }
        } catch (error) {
            simulationMessage.textContent = `❌ Network error: ${error.message}`;
            simulationMessage.className = 'message error';
        }
        
        // Clear message after 5 seconds
        setTimeout(() => {
            simulationMessage.className = 'message';
            simulationMessage.textContent = '';
        }, 5000);
    });
}

// Start auto-refresh
function startAutoRefresh() {
    if (refreshIntervalId) clearInterval(refreshIntervalId);
    refreshIntervalId = setInterval(() => {
        if (autoRefresh) {
            updateDashboard();
        }
    }, REFRESH_INTERVAL);
}

// Stop auto-refresh
function stopAutoRefresh() {
    if (refreshIntervalId) {
        clearInterval(refreshIntervalId);
        refreshIntervalId = null;
    }
}

// Update entire dashboard
async function updateDashboard() {
    try {
        const [status, tasks, workers, workerStats] = await Promise.all([
            fetch(`${API_BASE}/api/status`).then(r => r.json()),
            fetch(`${API_BASE}/api/tasks`).then(r => r.json()),
            fetch(`${API_BASE}/api/workers`).then(r => r.json()),
            fetch(`${API_BASE}/api/worker_stats`).then(r => r.json())
        ]);

        currentTasks = tasks.tasks || [];
        updateStatistics(status);
        updateTaskTable(currentTasks);
        updateWorkers(workers);
        updateCharts(status, currentTasks);
        updateWorkerChart(workerStats);
        updateLastUpdateTime();
        
    } catch (error) {
        console.error('Error updating dashboard:', error);
        document.getElementById('statusIndicator').style.color = '#e74c3c';
    }
}

// Update statistics cards
function updateStatistics(status) {
    animateValue('totalTasks', status.total_tasks);
    animateValue('runningTasks', status.running_tasks);
    animateValue('pendingTasks', status.pending_tasks);
    animateValue('completedTasks', status.completed_tasks);
    animateValue('failedTasks', status.failed_tasks);
    animateValue('activeWorkers', status.active_workers);
}

// Animate value changes
function animateValue(elementId, newValue) {
    const element = document.getElementById(elementId);
    const oldValue = parseInt(element.textContent) || 0;
    
    if (oldValue === newValue) return;
    
    const duration = 500;
    const startTime = Date.now();
    const difference = newValue - oldValue;
    
    function update() {
        const elapsed = Date.now() - startTime;
        const progress = Math.min(elapsed / duration, 1);
        const easeOutQuart = 1 - Math.pow(1 - progress, 4);
        const current = Math.round(oldValue + difference * easeOutQuart);
        
        element.textContent = current;
        
        if (progress < 1) {
            requestAnimationFrame(update);
        } else {
            element.textContent = newValue;
        }
    }
    
    update();
}

// Update task table
let previousTasks = new Map();

function updateTaskTable(tasks) {
    const tbody = document.getElementById('tasksTableBody');
    const statusFilter = document.getElementById('statusFilter').value;
    const priorityFilter = document.getElementById('priorityFilter').value;
    
    // Filter tasks
    let filteredTasks = tasks;
    if (statusFilter !== 'all') {
        filteredTasks = filteredTasks.filter(t => t.status === statusFilter);
    }
    if (priorityFilter !== 'all') {
        filteredTasks = filteredTasks.filter(t => t.priority === priorityFilter);
    }
    
    // Sort by ID descending (newest first)
    filteredTasks.sort((a, b) => b.id - a.id);
    
    if (filteredTasks.length === 0) {
        tbody.innerHTML = '<tr><td colspan="8" class="loading">No tasks found</td></tr>';
        return;
    }
    
    let html = '';
    
    filteredTasks.forEach(task => {
        const isNew = !previousTasks.has(task.id);
        const rowClass = isNew ? 'new-task' : '';
        
        const canCancel = task.status === 'PENDING';
        html += `
            <tr class="${rowClass}">
                <td>${task.id}</td>
                <td class="task-name-cell" onclick="openTaskModal(${task.id})" style="cursor:pointer;">${escapeHtml(task.name)}</td>
                <td><span class="priority-badge priority-${task.priority.toLowerCase()}">${task.priority}</span></td>
                <td><span class="status-badge status-${task.status.toLowerCase()}">${task.status}</span></td>
                <td>
                    <div class="progress-bar">
                        <div class="progress-fill" style="width: ${task.progress || 0}%"></div>
                    </div>
                </td>
                <td>${task.worker_id >= 0 ? `Worker ${task.worker_id}` : '-'}</td>
                <td>${formatTime(task.creation_time)}</td>
                <td class="task-actions">
                    <button class="btn btn-view" onclick="openTaskModal(${task.id})">👁</button>
                    <button class="btn btn-cancel" onclick="cancelTask(${task.id})" ${canCancel ? '' : 'disabled'}>✖</button>
                </td>
            </tr>
        `;
    });
    
    tbody.innerHTML = html;
    previousTasks = new Map(tasks.map(t => [t.id, t]));
}

// Update workers section
function updateWorkers(workers) {
    const grid = document.getElementById('workersGrid');
    const active = workers.active_workers || 0;
    const total = workers.total_workers || 0;
    
    let html = '';
    for (let i = 0; i < total; i++) {
        const isActive = i < active;
        html += `
            <div class="worker-card">
                <h4>Worker ${i}</h4>
                <div class="worker-info">
                    <span>Status: <strong style="color: ${isActive ? '#2ecc71' : '#e74c3c'}">
                        ${isActive ? 'Active' : 'Inactive'}
                    </strong></span>
                </div>
            </div>
        `;
    }
    
    grid.innerHTML = html || '<div class="loading">No workers</div>';
}

// Update charts
let lastCompletedCount = null;

function updateCharts(status, tasks) {
    // Update throughput chart
    const now = new Date();
    const timeLabel = now.toLocaleTimeString();
    
    // Calculate difference (handle initial load)
    let completedDiff = 0;
    if (lastCompletedCount !== null) {
        completedDiff = Math.max(0, status.completed_tasks - lastCompletedCount);
    }
    lastCompletedCount = status.completed_tasks;
    
    throughputChart.data.labels.push(timeLabel);
    throughputChart.data.datasets[0].data.push(completedDiff);
    
    // Keep only last N data points
    if (throughputChart.data.labels.length > maxDataPoints) {
        throughputChart.data.labels.shift();
        throughputChart.data.datasets[0].data.shift();
    }
    
    throughputChart.update('none'); // 'none' for smooth animation
    
    // Update status distribution chart
    statusChart.data.datasets[0].data = [
        status.pending_tasks,
        status.running_tasks,
        status.completed_tasks,
        status.failed_tasks
    ];
    
    statusChart.update('none');
}

// Update last update time
function updateLastUpdateTime() {
    const now = new Date();
    document.getElementById('lastUpdate').textContent = 
        `Last updated: ${now.toLocaleTimeString()}`;
    document.getElementById('statusIndicator').style.color = '#2ecc71';
}

// Utility functions
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function formatTime(timeStr) {
    if (!timeStr) return '-';
    const date = new Date(timeStr);
    return date.toLocaleString();
}

// Update worker utilization chart
function updateWorkerChart(workerStats) {
    if (!workerStats || !workerStats.workers) return;
    
    const labels = workerStats.workers.map(w => `Worker ${w.id}`);
    const runningData = workerStats.workers.map(w => w.running);
    const completedData = workerStats.workers.map(w => w.completed);
    
    workerChart.data.labels = labels;
    workerChart.data.datasets[0].data = runningData;
    workerChart.data.datasets[1].data = completedData;
    workerChart.update('none');
}

// Export functions
async function exportCSV() {
    try {
        const response = await fetch('/api/export/csv');
        const csv = await response.text();
        downloadFile(csv, 'tasks.csv', 'text/csv');
    } catch (error) {
        console.error('Export CSV failed:', error);
        alert('Failed to export CSV');
    }
}

async function exportJSON() {
    try {
        const response = await fetch('/api/export/json');
        const json = await response.text();
        downloadFile(json, 'tasks.json', 'application/json');
    } catch (error) {
        console.error('Export JSON failed:', error);
        alert('Failed to export JSON');
    }
}

function downloadFile(content, filename, type) {
    const blob = new Blob([content], { type });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

// Task modal functions
function openTaskModal(taskId) {
    const task = currentTasks.find(t => t.id === taskId);
    if (!task) return;
    
    document.getElementById('modalTaskName').textContent = task.name;
    document.getElementById('modalTaskId').textContent = task.id;
    document.getElementById('modalTaskPriority').innerHTML = `<span class="priority-badge priority-${task.priority.toLowerCase()}">${task.priority}</span>`;
    document.getElementById('modalTaskStatus').innerHTML = `<span class="status-badge status-${task.status.toLowerCase()}">${task.status}</span>`;
    document.getElementById('modalTaskWorker').textContent = task.worker_id >= 0 ? `Worker ${task.worker_id}` : 'Not assigned';
    document.getElementById('modalTaskDuration').textContent = `${task.execution_time_ms} ms`;
    document.getElementById('modalTaskProgress').textContent = `${(task.progress || 0).toFixed(1)}%`;
    
    // Timeline
    document.getElementById('modalTimeCreated').textContent = task.creation_time || '-';
    document.getElementById('modalTimeStarted').textContent = task.start_time || '-';
    document.getElementById('modalTimeEnded').textContent = task.end_time || '-';
    
    // Calculate metrics
    let waitTime = '-', execTime = '-', turnaroundTime = '-';
    
    if (task.creation_time && task.start_time) {
        const created = new Date(task.creation_time);
        const started = new Date(task.start_time);
        const wait = (started - created) / 1000;
        waitTime = `${wait.toFixed(1)}s`;
    }
    
    if (task.start_time && task.end_time) {
        const started = new Date(task.start_time);
        const ended = new Date(task.end_time);
        const exec = (ended - started) / 1000;
        execTime = `${exec.toFixed(1)}s`;
    }
    
    if (task.creation_time && task.end_time) {
        const created = new Date(task.creation_time);
        const ended = new Date(task.end_time);
        const turnaround = (ended - created) / 1000;
        turnaroundTime = `${turnaround.toFixed(1)}s`;
    }
    
    document.getElementById('modalWaitTime').textContent = waitTime;
    document.getElementById('modalExecTime').textContent = execTime;
    document.getElementById('modalTurnaroundTime').textContent = turnaroundTime;
    
    document.getElementById('taskModal').classList.add('active');
}

function closeModal() {
    document.getElementById('taskModal').classList.remove('active');
}

// Cancel task function
async function cancelTask(taskId) {
    if (!confirm(`Cancel task ${taskId}?`)) return;
    
    try {
        const response = await fetch('/api/cancel_task', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ task_id: taskId })
        });
        
        const result = await response.json();
        
        if (result.success) {
            updateDashboard();
        } else {
            alert(result.error || 'Failed to cancel task');
        }
    } catch (error) {
        console.error('Cancel task failed:', error);
        alert('Network error');
    }
}

// Handle page visibility (pause when tab is hidden)
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        stopAutoRefresh();
    } else if (autoRefresh) {
        startAutoRefresh();
        updateDashboard();
    }
});

