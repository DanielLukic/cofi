# Makefile for cofi - C/GTK window switcher

CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -g -Wno-deprecated-declarations $(shell pkg-config --cflags gtk+-3.0 x11 gio-2.0)
CXXFLAGS = $(CFLAGS) -std=c++11 -Iinclude
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 x11 gio-2.0) -lm -lXrandr -lstdc++

# Build number from environment (GitHub Actions) or default to 0
BUILD_NUMBER ?= 0
CFLAGS += -DBUILD_NUMBER=$(BUILD_NUMBER)

# Source files
SOURCES = src/main.c \
          src/x11_utils.c \
          src/window_list.c \
          src/history.c \
          src/display.c \
          src/filter.c \
          src/log.c \
          src/x11_events.c \
          src/instance.c \
          src/harpoon.c \
          src/config.c \
          src/harpoon_config.c \
          src/window_matcher.c \
          src/match.c \
          src/utils.c \
          src/cli_args.cpp \
          src/gtk_window.c \
          src/app_init.c \
          src/command_mode.c \
          src/monitor_move.c \
          src/selection.c \
          src/workarea.c \
          src/size_hints.c \
          src/overlay_manager.c \
          src/tiling.c \
          src/dbus_service.c

# Separate C and C++ sources
C_SOURCES = $(filter %.c,$(SOURCES))
CPP_SOURCES = $(filter %.cpp,$(SOURCES))

# Object files
C_OBJECTS = $(C_SOURCES:.c=.o)
CPP_OBJECTS = $(CPP_SOURCES:.cpp=.o)
OBJECTS = $(C_OBJECTS) $(CPP_OBJECTS)

# Target executable
TARGET = cofi

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile C source files
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ source files
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -f test/test_command_parsing test/test_window_matcher


# Install target (optional)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Uninstall target
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Debug build with debug output enabled
debug: CFLAGS += -DDEBUG
debug: clean $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Test targets
test: test_window_matcher test_command_parsing
	cd test && ./run_tests.sh

# Build command parsing test
test_command_parsing: test/test_command_parsing.c
	$(CC) $(CFLAGS) -o test/test_command_parsing test/test_command_parsing.c

# Quick test targets for development
test_quick: src/match.o
	@if [ -f test/test_ddl.c ]; then \
		$(CC) $(CFLAGS) -o test/test_ddl test/test_ddl.c src/match.o $(LDFLAGS) 2>/dev/null && \
		echo "Running DDL test:" && ./test/test_ddl; \
	fi
	@if [ -f test/test_word_boundaries.c ]; then \
		$(CC) $(CFLAGS) -o test/test_word_boundaries test/test_word_boundaries.c src/match.o $(LDFLAGS) 2>/dev/null && \
		echo "Running word boundaries test:" && ./test/test_word_boundaries; \
	fi

# Integration tests
test_window_matcher: test/test_window_matcher.c src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_window_matcher test/test_window_matcher.c src/window_matcher.o src/log.o $(LDFLAGS)

test_harpoon_integration: test/test_harpoon_integration.c src/harpoon.o src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_harpoon_integration test/test_harpoon_integration.c src/harpoon.o src/window_matcher.o src/log.o $(LDFLAGS)

test_display_integration: test/test_display_integration.c src/harpoon.o src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_display_integration test/test_display_integration.c src/harpoon.o src/window_matcher.o src/log.o $(LDFLAGS)

test_event_sequence: test/test_event_sequence.c src/harpoon.o src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_event_sequence test/test_event_sequence.c src/harpoon.o src/window_matcher.o src/log.o $(LDFLAGS)

clean_tests:
	rm -f test/test_* test/*.o

.PHONY: all clean install uninstall debug run test build_tests clean_tests