#!/bin/bash

# Source backup utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config_backup.sh"

# Backup config at start
backup_config

# Restore config on exit
trap 'restore_config' EXIT INT TERM

# Kill any existing cofi instance
pkill cofi || true

# Start cofi in background with debug logging to a file
./cofi --log-level debug 2>&1 | tee cofi_debug.log &
COFI_PID=$!

# Wait a moment for cofi to start
sleep 1

# Now let's check what's in the debug log
echo "=== Initial window list and reassignment check ==="
grep -E "(Looking for:|Checking window|Automatically reassigned|Window 0x.*no longer exists)" cofi_debug.log || echo "No matching entries yet"

# Keep monitoring the log
echo -e "\n=== Monitoring for reassignment activity ==="
echo "Cofi is running with PID $COFI_PID"
echo "You can check the log with: tail -f cofi_debug.log | grep -E '(Looking for:|Checking window|reassigned)'"
echo "To stop cofi: kill $COFI_PID"