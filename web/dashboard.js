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

// Initialize dashboard
document.addEventListener('DOMContentLoaded', () => {
    initializeCharts();
    setupEventListeners();
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

    document.getElementById('statusFilter').addEventListener('change', updateTaskTable);
    document.getElementById('priorityFilter').addEventListener('change', updateTaskTable);
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
        const [status, tasks, workers] = await Promise.all([
            fetch(`${API_BASE}/api/status`).then(r => r.json()),
            fetch(`${API_BASE}/api/tasks`).then(r => r.json()),
            fetch(`${API_BASE}/api/workers`).then(r => r.json())
        ]);

        updateStatistics(status);
        updateTaskTable(tasks.tasks || []);
        updateWorkers(workers);
        updateCharts(status, tasks.tasks || []);
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
        tbody.innerHTML = '<tr><td colspan="7" class="loading">No tasks found</td></tr>';
        return;
    }
    
    let html = '';
    
    filteredTasks.forEach(task => {
        const isNew = !previousTasks.has(task.id);
        const rowClass = isNew ? 'new-task' : '';
        
        html += `
            <tr class="${rowClass}">
                <td>${task.id}</td>
                <td>${escapeHtml(task.name)}</td>
                <td><span class="priority-badge priority-${task.priority.toLowerCase()}">${task.priority}</span></td>
                <td><span class="status-badge status-${task.status.toLowerCase()}">${task.status}</span></td>
                <td>
                    <div class="progress-bar">
                        <div class="progress-fill" style="width: ${task.progress || 0}%"></div>
                    </div>
                </td>
                <td>${task.worker_id >= 0 ? `Worker ${task.worker_id}` : '-'}</td>
                <td>${formatTime(task.creation_time)}</td>
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

// Handle page visibility (pause when tab is hidden)
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        stopAutoRefresh();
    } else if (autoRefresh) {
        startAutoRefresh();
        updateDashboard();
    }
});

