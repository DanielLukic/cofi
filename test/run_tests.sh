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

# Run config round-trip tests if they exist
if [ -f test_config_roundtrip ]; then
    echo ""
    echo "Running config round-trip tests..."
    ./test_config_roundtrip
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run config set/display tests if they exist
if [ -f test_config_set ]; then
    echo ""
    echo "Running config set/display tests..."
    ./test_config_set
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run hotkey config tests if they exist
if [ -f test_hotkey_config ]; then
    echo ""
    echo "Running hotkey config tests..."
    ./test_hotkey_config
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run fzf algorithm tests if they exist
if [ -f test_fzf_algo ]; then
    echo ""
    echo "Running fzf algorithm tests..."
    ./test_fzf_algo
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run named window tests if they exist
if [ -f test_named_window ]; then
    echo ""
    echo "Running named window tests..."
    ./test_named_window
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run match scoring tests if they exist
if [ -f test_match_scoring ]; then
    echo ""
    echo "Running match scoring tests..."
    ./test_match_scoring
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run command alias tests if they exist
if [ -f test_command_aliases ]; then
    echo ""
    echo "Running command alias tests..."
    ./test_command_aliases
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run shortcut parser tests if they exist
if [ -f test_parse_shortcut ]; then
    echo ""
    echo "Running shortcut parser tests..."
    ./test_parse_shortcut
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run wildcard match tests if they exist
if [ -f test_wildcard_match ]; then
    echo ""
    echo "Running wildcard match tests..."
    ./test_wildcard_match
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run command dispatch tests if they exist
if [ -f test_command_dispatch ]; then
    echo ""
    echo "Running command dispatch tests..."
    ./test_command_dispatch
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

# Run rules tests if they exist
if [ -f test_rules ]; then
    echo ""
    echo "Running rules tests..."
    ./test_rules
    if [ $? -ne 0 ]; then
        overall_exit=1
    fi
fi

exit $overall_exit
