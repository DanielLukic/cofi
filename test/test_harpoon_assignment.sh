#!/bin/bash

# Test script for harpoon slot assignment and reassignment
# Tests that window reassignment works when a window with same properties is reopened

set -e

# Source backup utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config_backup.sh"

LOG_FILE="cofi_test.log"
TERMINAL_TITLE="TEST-TERM-COFI"

# Backup config at start
backup_config

cleanup() {
    echo "Cleaning up..."
    # Kill cofi
    pkill -f cofi || true
    # Kill test terminal by title
    wmctrl -c "$TERMINAL_TITLE" || true
    # Remove lock file
    rm -f /tmp/cofi.lock
    # Restore original config
    restore_config
    echo "Cleanup complete"
}

# Cleanup on exit
trap cleanup EXIT

# Kill any existing cofi instance
echo "Killing any existing cofi instances..."
./cofi --kill || true
sleep 1

echo "=== Starting Harpoon Assignment Test ==="
echo

# Phase 1: Start cofi and create test terminal
echo "Phase 1: Starting cofi with debug logging..."
./cofi --log-level debug --log-file "$LOG_FILE" &
COFI_PID=$!
echo "Cofi started with PID: $COFI_PID"

# Wait for cofi to start
sleep 2

echo "Phase 2: Starting test terminal..."
gnome-terminal --title="$TERMINAL_TITLE" -- bash -c "echo 'Test terminal for harpoon assignment'; exec bash" &
TERMINAL_PID=$!
echo "Terminal started with PID: $TERMINAL_PID"

# Wait for terminal to appear
sleep 2

echo "Phase 3: Monitoring for harpoon slot assignment..."
echo "Please manually:"
echo "1. Press a key to open cofi"
echo "2. Press a number key (1-9) to assign the '$TERMINAL_TITLE' window to a harpoon slot"
echo "3. Press Enter or Escape to close cofi"
echo

# Monitor log for assignment
echo "Watching log for slot assignment..."
timeout 60 tail -f "$LOG_FILE" | while read line; do
    echo "LOG: $line"
    if echo "$line" | grep -q "Assigned window.*$TERMINAL_TITLE.*to slot"; then
        echo "=== ASSIGNMENT DETECTED ==="
        break
    fi
done

# Extract the assigned slot number from log
ASSIGNED_SLOT=$(grep "Assigned window.*$TERMINAL_TITLE.*to slot" "$LOG_FILE" | tail -1 | sed -n 's/.*to slot \([0-9]\).*/\1/p')

if [ -z "$ASSIGNED_SLOT" ]; then
    echo "ERROR: No slot assignment detected. Please run manually and assign a slot."
    exit 1
fi

echo "Detected assignment to slot: $ASSIGNED_SLOT"
echo

# Phase 4: Kill terminal and restart it
echo "Phase 4: Killing test terminal..."
wmctrl -c "$TERMINAL_TITLE" || true
sleep 2

echo "Phase 5: Starting new terminal with same title..."
gnome-terminal --title="$TERMINAL_TITLE" -- bash -c "echo 'New test terminal for reassignment test'; exec bash" &
sleep 2

echo "Phase 6: Waiting for reassignment detection..."
echo "Watching log for automatic reassignment..."

# Monitor log for reassignment
timeout 30 tail -f "$LOG_FILE" | while read line; do
    echo "LOG: $line"
    if echo "$line" | grep -q "Automatically reassigned slot $ASSIGNED_SLOT"; then
        echo "=== REASSIGNMENT DETECTED ==="
        break
    fi
done

# Check if reassignment occurred
if grep -q "Automatically reassigned slot $ASSIGNED_SLOT" "$LOG_FILE"; then
    echo "✅ SUCCESS: Automatic reassignment detected!"
    echo "Test completed successfully."
else
    echo "❌ FAILURE: No automatic reassignment detected."
    echo "Check the log file: $LOG_FILE"
    exit 1
fi

echo
echo "=== Test Summary ==="
echo "1. Assigned window to slot: $ASSIGNED_SLOT"
echo "2. Killed and recreated terminal"
echo "3. Detected automatic reassignment"
echo "Full log available in: $LOG_FILE"