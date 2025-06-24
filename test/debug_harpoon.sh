#!/bin/bash

echo "=== Cofi Harpoon Debug Script ==="
echo "This script will help debug the harpoon reassignment issue"
echo

# Check if cofi exists
if [ ! -f "./cofi" ]; then
    echo "ERROR: cofi executable not found. Please run 'make' first."
    exit 1
fi

echo "1. Current config file contents:"
CONFIG_FILE="$HOME/.config/cofi.json"
if [ -f "$CONFIG_FILE" ]; then
    echo "   Config file exists: $CONFIG_FILE"
    cat "$CONFIG_FILE"
    echo
else
    echo "   Config file does not exist: $CONFIG_FILE"
    echo
fi

echo "2. Building debug version with logging enabled..."
make clean >/dev/null 2>&1
CFLAGS="-DDEBUG -g" make >/dev/null 2>&1

echo "3. Instructions for testing:"
echo "   a. Run: ./cofi"
echo "   b. Assign a window to a harpoon slot (press a number key 1-9)"
echo "   c. Note the window ID shown in the rightmost column"
echo "   d. Close that window"
echo "   e. Open the same application again"
echo "   f. Check if the window ID changes in the cofi display"
echo "   g. Close cofi (press Escape)"
echo
echo "4. After testing, check the debug output above and answer:"
echo "   - Did you see 'Automatically reassigned slot X' messages?"
echo "   - Did the window ID in the display change after reopening?"
echo "   - Does the config file now contain the new window ID?"
echo
echo "5. Config file after testing:"
echo "   Run: cat $CONFIG_FILE"
echo
echo "6. If the issue persists, run with full debug logging:"
echo "   LOG_LEVEL=DEBUG ./cofi 2>&1 | tee cofi_debug.log"
echo

echo "Ready to test. Press Enter to continue or Ctrl+C to exit."
read -r

echo "Starting cofi with debug output..."
./cofi