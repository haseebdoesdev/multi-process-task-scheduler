#!/bin/bash

# Start Web Dashboard Script
# This script starts the web server for the dashboard

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

# Check if scheduler is running
if [ ! -f scheduler.pid ]; then
    echo "Warning: Scheduler is not running. Start it with ./scripts/start_scheduler.sh"
    echo "The web dashboard will still start but won't show any data."
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Build web server if needed
if [ ! -f web_server ]; then
    echo "Building web server..."
    make web_server
    if [ $? -ne 0 ]; then
        echo "Error: Failed to build web server"
        exit 1
    fi
fi

# Check if web server is already running
if [ -f web_server.pid ]; then
    PID=$(cat web_server.pid)
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Web server is already running with PID $PID"
        echo "Dashboard available at http://localhost:8080"
        exit 0
    else
        rm -f web_server.pid
    fi
fi

# Check if web directory exists
if [ ! -d "web" ]; then
    echo "Error: web directory not found"
    exit 1
fi

# Start web server in background
echo "Starting web server..."
./web_server > logs/web_server.log 2>&1 &
WEB_PID=$!

# Wait a moment for server to start
sleep 1

# Check if server started successfully
if ps -p "$WEB_PID" > /dev/null 2>&1; then
    echo "$WEB_PID" > web_server.pid
    echo "Web server started successfully with PID $WEB_PID"
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  🌐 Web Dashboard is now running!"
    echo ""
    echo "  Open your browser and visit:"
    echo "  👉 http://localhost:8080"
    echo ""
    echo "  Press Ctrl+C here to stop the web server"
    echo "  (or run: ./scripts/stop_web_dashboard.sh)"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    
    # Wait for user interrupt
    trap "echo ''; echo 'Stopping web server...'; kill $WEB_PID 2>/dev/null; rm -f web_server.pid; exit 0" INT TERM
    wait $WEB_PID
else
    echo "Error: Web server failed to start. Check logs/web_server.log"
    rm -f web_server.pid
    exit 1
fi

