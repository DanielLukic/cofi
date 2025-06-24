#!/bin/bash

# Compile and run window matcher tests
echo "Compiling window matcher tests..."
gcc -Wall -Wextra -g -I../src test_window_matcher.c ../src/window_matcher.c -o test_window_matcher

if [ $? -eq 0 ]; then
    echo "Running tests..."
    ./test_window_matcher
    exit_code=$?
    rm -f test_window_matcher
    exit $exit_code
else
    echo "Compilation failed!"
    exit 1
fi