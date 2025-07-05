#!/bin/bash

# Track overall exit code
overall_exit=0

# Compile and run window matcher tests
echo "Compiling window matcher tests..."
gcc -Wall -Wextra -g -I../src test_window_matcher.c ../src/window_matcher.c -o test_window_matcher

if [ $? -eq 0 ]; then
    echo "Running window matcher tests..."
    ./test_window_matcher
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
    rm -f test_window_matcher
else
    echo "Window matcher compilation failed!"
    overall_exit=1
fi

# Run command parsing tests if they exist
if [ -f test_command_parsing ]; then
    echo ""
    echo "Running command parsing tests..."
    ./test_command_parsing
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

exit $overall_exit