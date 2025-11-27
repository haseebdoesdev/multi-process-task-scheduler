#!/bin/bash
# Complete setup script for WSL/Ubuntu
# This script fixes line endings and builds the project

set -e

echo "========================================"
echo "Multi-Process Task Scheduler Setup"
echo "========================================"
echo ""

# Step 1: Fix line endings in ALL shell scripts
echo "Step 1: Fixing line endings..."

# Fix all scripts using sed (works even without dos2unix)
for script in scripts/*.sh setup.sh; do
    if [ -f "$script" ]; then
        sed -i 's/\r$//' "$script" 2>/dev/null || true
    fi
done

# Try dos2unix if available (better results)
if command -v dos2unix >/dev/null 2>&1; then
    dos2unix scripts/*.sh setup.sh 2>/dev/null || true
    echo "✓ Line endings fixed using dos2unix"
else
    echo "✓ Line endings fixed using sed"
    echo "  (Install dos2unix for better results: sudo apt install dos2unix)"
fi

# Step 2: Make scripts executable
echo ""
echo "Step 2: Making scripts executable..."
chmod +x scripts/*.sh
chmod +x setup.sh 2>/dev/null || true
echo "✓ Scripts are now executable"

# Step 3: Install build tools if needed
echo ""
echo "Step 3: Checking build tools..."
if ! command -v gcc &> /dev/null; then
    echo "Installing build tools..."
    sudo apt update
    sudo apt install -y build-essential make gcc
else
    echo "✓ Build tools already installed"
fi

# Step 4: Build the project
echo ""
echo "Step 4: Building project..."
make clean 2>/dev/null || true
make

if [ -f scheduler ] && [ -f worker ] && [ -f web_server ]; then
    echo "✓ Build successful!"
    echo "  - scheduler: $(ls -lh scheduler | awk '{print $5}')"
    echo "  - worker: $(ls -lh worker | awk '{print $5}')"
    echo "  - web_server: $(ls -lh web_server | awk '{print $5}')"
else
    echo "✗ Build failed! Check error messages above."
    exit 1
fi

# Step 5: Create logs directory
echo ""
echo "Step 5: Creating logs directory..."
mkdir -p logs
echo "✓ Logs directory created"

echo ""
echo "========================================"
echo "Setup Complete!"
echo "========================================"
echo ""
echo "You can now run the project:"
echo ""
echo "1. Start scheduler:"
echo "   ./scripts/start_scheduler.sh"
echo ""
echo "2. Add tasks:"
echo "   ./scripts/add_task.sh \"Task Name\" HIGH 5000"
echo ""
echo "3. Start web dashboard:"
echo "   ./scripts/start_web_dashboard.sh"
echo "   Then open http://localhost:8080 in your browser"
echo ""
echo "4. Monitor in terminal:"
echo "   ./scripts/monitor.sh"
echo ""
echo "5. Clean up:"
echo "   ./scripts/cleanup.sh"
echo ""

