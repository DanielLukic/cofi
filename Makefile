# Makefile for cofi - C/GTK window switcher

CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -g -Wno-deprecated-declarations -MMD -MP $(shell pkg-config --cflags gtk+-3.0 x11 gio-2.0)
CXXFLAGS = $(CFLAGS) -std=c++11 -Iinclude
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 x11 gio-2.0) -lm -lXrandr -lXfixes -lXft -lstdc++

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
          src/harpoon.c \
          src/config.c \
          src/harpoon_config.c \
          src/window_matcher.c \
          src/named_window.c \
          src/named_window_config.c \
          src/filter_names.c \
          src/match.c \
          src/utils.c \
          src/cli_args.cpp \
          src/gtk_window.c \
          src/app_init.c \
          src/command_mode.c \
          src/command_parser.c \
          src/monitor_move.c \
          src/selection.c \
          src/workarea.c \
          src/size_hints.c \
          src/overlay_manager.c \
          src/tiling_overlay.c \
          src/workspace_overlay.c \
          src/workspace_rename_overlay.c \
          src/harpoon_overlay.c \
          src/tiling.c \
          src/atom_cache.c \
          src/dynamic_display.c \
          src/frame_extents.c \
          src/workspace_utils.c \
          src/gtk_utils.c \
          src/workspace_slots.c \
          src/slot_overlay.c \
          src/fzf_algo.c \
          src/window_highlight.c \
          src/hotkeys.c \
          src/hotkey_config.c \
          src/rules_config.c \
          src/rules.c

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
	rm -f $(OBJECTS) $(TARGET) src/*.d
	rm -f test/test_command_parsing test/test_window_matcher


PREFIX ?= $(HOME)/.local
BINDIR = $(PREFIX)/bin
SYSTEMD_USER_DIR = $(HOME)/.config/systemd/user

# Install binary + systemd user service
install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/
	install -d $(SYSTEMD_USER_DIR)
	@echo '[Unit]' > $(SYSTEMD_USER_DIR)/cofi.service
	@echo 'Description=Cofi window switcher' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo 'After=graphical-session.target' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo '' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo '[Service]' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo 'Type=simple' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo 'ExecStart=$(BINDIR)/cofi' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo 'Restart=on-failure' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo 'RestartSec=1' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo '' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo '[Install]' >> $(SYSTEMD_USER_DIR)/cofi.service
	@echo 'WantedBy=default.target' >> $(SYSTEMD_USER_DIR)/cofi.service
	systemctl --user daemon-reload
	systemctl --user enable --now cofi
	@echo "Installed to $(BINDIR)/cofi and enabled systemd user service"

# Uninstall binary + systemd service
uninstall:
	-systemctl --user disable --now cofi
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(SYSTEMD_USER_DIR)/cofi.service
	systemctl --user daemon-reload
	@echo "Uninstalled cofi"

# Debug build with debug output enabled
debug: CFLAGS += -DDEBUG
debug: clean $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Test targets
test: test_window_matcher test_command_parsing test_config_roundtrip test_config_set test_hotkey_config test_fzf_algo test_named_window test_match_scoring test_command_aliases test_wildcard_match test_parse_shortcut test_scrollbar test_rules test_command_dispatch test_dynamic_display_fixed
	cd test && ./run_tests.sh

# Build command parsing test
test_command_parsing: test/test_command_parsing.c src/command_parser.o
	$(CC) $(CFLAGS) -o test/test_command_parsing test/test_command_parsing.c src/command_parser.o $(LDFLAGS)

# Build config round-trip test
test_config_roundtrip: test/test_config_roundtrip.c src/config.o src/log.o src/utils.o
	$(CC) $(CFLAGS) -o test/test_config_roundtrip test/test_config_roundtrip.c src/config.o src/log.o src/utils.o $(LDFLAGS)

# Build config set/display test
test_config_set: test/test_config_set.c src/config.o src/log.o src/utils.o
	$(CC) $(CFLAGS) -o test/test_config_set test/test_config_set.c src/config.o src/log.o src/utils.o $(LDFLAGS)

# Build hotkey config test
test_hotkey_config: test/test_hotkey_config.c src/hotkey_config.o src/log.o
	$(CC) $(CFLAGS) -o test/test_hotkey_config test/test_hotkey_config.c src/hotkey_config.o src/log.o $(LDFLAGS)

# Build fzf algorithm test
test_fzf_algo: test/test_fzf_algo.c src/fzf_algo.o
	$(CC) $(CFLAGS) -o test/test_fzf_algo test/test_fzf_algo.c src/fzf_algo.o $(LDFLAGS)

# Build named window test
test_named_window: test/test_named_window.c src/named_window.o src/window_matcher.o src/log.o src/utils.o
	$(CC) $(CFLAGS) -o test/test_named_window test/test_named_window.c src/named_window.o src/window_matcher.o src/log.o src/utils.o $(LDFLAGS)

# Build match scoring test (fzy algorithm)
test_match_scoring: test/test_match_scoring.c src/match.o
	$(CC) $(CFLAGS) -o test/test_match_scoring test/test_match_scoring.c src/match.o $(LDFLAGS)

# Build command alias edge case test
test_command_aliases: test/test_command_aliases.c src/command_parser.o
	$(CC) $(CFLAGS) -o test/test_command_aliases test/test_command_aliases.c src/command_parser.o $(LDFLAGS)

# Build wildcard match test
test_wildcard_match: test/test_wildcard_match.c src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_wildcard_match test/test_wildcard_match.c src/window_matcher.o src/log.o $(LDFLAGS)

# Build parse shortcut test
test_parse_shortcut: test/test_parse_shortcut.c src/utils.o
	$(CC) $(CFLAGS) -o test/test_parse_shortcut test/test_parse_shortcut.c src/utils.o $(LDFLAGS)

# Build command dispatch test
test_command_dispatch: test/test_command_dispatch.c
	$(CC) $(CFLAGS) -o test/test_command_dispatch test/test_command_dispatch.c $(LDFLAGS)

# Build rules test
test_rules: test/test_rules.c src/rules_config.o src/rules.o src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_rules test/test_rules.c src/rules_config.o src/rules.o src/window_matcher.o src/log.o $(LDFLAGS)

# Build scrollbar overlay test (extracts scrollbar functions only)
test_scrollbar: test/test_scrollbar.c
	$(CC) $(CFLAGS) -DSCROLLBAR_TEST_STANDALONE -o test/test_scrollbar test/test_scrollbar.c $(LDFLAGS)

# Build fixed window sizing tests
test_dynamic_display_fixed: test/test_dynamic_display_fixed.c src/dynamic_display.o src/log.o
	$(CC) $(CFLAGS) -o test/test_dynamic_display_fixed test/test_dynamic_display_fixed.c src/dynamic_display.o src/log.o $(LDFLAGS)

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

-include $(wildcard src/*.d)

.PHONY: all clean install uninstall debug run test build_tests clean_tests
