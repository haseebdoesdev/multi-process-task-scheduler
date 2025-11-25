#!/bin/bash

# Stop Web Dashboard Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

if [ ! -f web_server.pid ]; then
    echo "Web server is not running"
    exit 0
fi

PID=$(cat web_server.pid)
if ps -p "$PID" > /dev/null 2>&1; then
    echo "Stopping web server (PID: $PID)..."
    kill "$PID"
    sleep 1
    if ps -p "$PID" > /dev/null 2>&1; then
        kill -9 "$PID"
    fi
    rm -f web_server.pid
    echo "Web server stopped"
else
    echo "Web server is not running (stale PID file)"
    rm -f web_server.pid
fi

