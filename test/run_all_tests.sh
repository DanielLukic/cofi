#!/bin/bash

# COFI Test Suite Runner
# Runs all tests and reports results

set -e  # Exit on first failure

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."  # Go to project root

echo "=== COFI Test Suite ==="
echo

# Build the main project first
echo "Building cofi..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1
echo "✓ Build successful"
echo

# Function to run a test and report result
run_test() {
    local test_name=$1
    local test_cmd=$2
    
    printf "%-40s" "$test_name..."
    if $test_cmd > /tmp/test_output.txt 2>&1; then
        echo "✓ PASSED"
    else
        echo "✗ FAILED"
        echo "  Output:"
        cat /tmp/test_output.txt | sed 's/^/    /'
        return 1
    fi
}

# Compile new unit tests if needed
echo "Compiling unit tests..."
if [ -f "test/test_filter.c" ]; then
    gcc -o test/test_filter test/test_filter.c src/filter.c src/match.c src/log.c $(pkg-config --cflags --libs gtk+-3.0 x11) -lm 2>/dev/null || echo "Warning: Failed to compile test_filter"
fi

if [ -f "test/test_history.c" ]; then
    gcc -o test/test_history test/test_history.c src/history.c src/log.c $(pkg-config --cflags --libs gtk+-3.0 x11) -lm 2>/dev/null || echo "Warning: Failed to compile test_history"
fi

# Run unit tests
echo "=== Unit Tests ==="

# Core functionality tests
if [ -f "test/test_filter" ]; then
    run_test "Filter (multi-stage matching)" "test/test_filter"
fi

if [ -f "test/test_history" ]; then
    run_test "History (MRU and Alt-Tab)" "test/test_history"
fi

# Matching algorithm tests
if [ -f "test/test_fuzzy" ]; then
    run_test "Fuzzy matching" "test/test_fuzzy"
fi

if [ -f "test/test_exact" ]; then
    run_test "Exact matching" "test/test_exact"
fi

if [ -f "test/test_ddl" ]; then
    run_test "DDL matching" "test/test_ddl"
fi

if [ -f "test/test_initials" ]; then
    run_test "Initials matching" "test/test_initials"
fi

if [ -f "test/test_word_boundaries" ]; then
    run_test "Word boundary matching" "test/test_word_boundaries"
fi

if [ -f "test/test_normalized" ]; then
    run_test "Normalized matching" "test/test_normalized"
fi

echo

# Run integration tests
echo "=== Integration Tests ==="

# Window matcher tests
if [ -f "test/test_window_matcher" ]; then
    run_test "Window matcher" "test/test_window_matcher"
fi

# Harpoon tests
if [ -f "test/test_harpoon_integration" ]; then
    run_test "Harpoon integration" "test/test_harpoon_integration"
fi

# Display tests
if [ -f "test/test_display_integration" ]; then
    run_test "Display integration" "test/test_display_integration"
fi

# Event sequence tests
if [ -f "test/test_event_sequence" ]; then
    run_test "Event sequence" "test/test_event_sequence"
fi

echo

# Run shell script tests
echo "=== Shell Script Tests ==="

if [ -f "test/test_display_id_bug.sh" ]; then
    run_test "Display ID bug test" "bash test/test_display_id_bug.sh"
fi

# Note: The following tests require manual interaction and start the cofi app
# They are excluded from automated testing but can be run manually:
# - test/test_harpoon_assignment.sh - Tests harpoon slot assignment with gnome-terminal
# - test/test_reassignment.sh - Tests window reassignment with cofi running

echo
echo "=== Test Suite Complete ==="

# Clean up
rm -f /tmp/test_output.txt