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
          src/app_setup.c \
          src/tab_switching.c \
          src/key_handler.c \
          src/key_handler_harpoon.c \
          src/key_handler_tabs.c \
          src/window_lifecycle.c \
          src/hotkey_dispatch.c \
          src/command_mode.c \
          src/run_mode.c \
          src/apps.c \
          src/system_actions.c \
          src/command_handlers.c \
          src/command_handlers_window.c \
          src/command_handlers_workspace.c \
          src/command_handlers_tiling.c \
          src/command_handlers_ui.c \
          src/command_parser.c \
          src/monitor_move.c \
          src/selection.c \
          src/workarea.c \
          src/size_hints.c \
          src/overlay_manager.c \
          src/overlay_dispatch.c \
          src/overlay_hotkey_add.c \
          src/overlay_hotkey_add_policy.c \
          src/overlay_hotkey_edit.c \
          src/overlay_harpoon.c \
          src/overlay_name.c \
          src/overlay_config.c \
          src/overlay_workspace.c \
          src/tiling_overlay.c \
          src/workspace_overlay.c \
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
          src/hotkey_grab_state.c \
          src/hotkey_config.c \
          src/rules_config.c \
          src/rules.c \
          src/display_pipeline.c \
          src/repeat_action.c \
          src/daemon_socket.c \
          src/daemon_socket_runtime.c

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

# Compile testable main object (renamed entrypoint to avoid collision in tests)
src/main_testable.o: src/main.c
	$(CC) $(CFLAGS) -Dmain=cofi_main_entry -c $< -o $@

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
test: test_window_matcher test_command_parsing test_command_parser_execution test_config_roundtrip test_config_set test_hotkey_config test_fzf_algo test_named_window test_match_scoring test_command_aliases test_wildcard_match test_parse_shortcut test_scrollbar test_rules test_command_dispatch test_dynamic_display_fixed test_display_pipeline test_overlay_dispatch test_hotkey_grab_state test_command_handlers_split test_command_handlers_behavior test_main_split_regression test_key_handler_core test_key_handler_harpoon test_key_handler_tabs test_workspace_slots_cap test_workspace_slots_occlusion test_repeat_action test_run_mode test_cli_args_run test_filter_ranking test_apps test_system_actions test_command_mode_targeting test_daemon_socket test_daemon_socket_dispatch test_cli_args_delegate test_tab_visibility test_command_candidates
	cd test && ./run_tests.sh

# Build command parsing test
test_command_parsing: test/test_command_parsing.c src/command_parser.o
	$(CC) $(CFLAGS) -o test/test_command_parsing test/test_command_parsing.c src/command_parser.o $(LDFLAGS)

# Build command parser execution-path test
test_command_parser_execution: test/test_command_parser_execution.c src/command_parser.o
	$(CC) $(CFLAGS) -o test/test_command_parser_execution test/test_command_parser_execution.c src/command_parser.o $(LDFLAGS)

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
test_command_dispatch: test/test_command_dispatch.c src/command_parser.o
	$(CC) $(CFLAGS) -DCOMMAND_POLICY_ONLY -o test/test_command_dispatch test/test_command_dispatch.c src/command_parser.o src/command_handlers.c $(LDFLAGS)

# Build rules test
test_rules: test/test_rules.c src/rules_config.o src/rules.o src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_rules test/test_rules.c src/rules_config.o src/rules.o src/window_matcher.o src/log.o $(LDFLAGS)

# Build scrollbar overlay test (extracts scrollbar functions only)
test_scrollbar: test/test_scrollbar.c
	$(CC) $(CFLAGS) -DSCROLLBAR_TEST_STANDALONE -o test/test_scrollbar test/test_scrollbar.c $(LDFLAGS)

# Build fixed window sizing tests
test_dynamic_display_fixed: test/test_dynamic_display_fixed.c src/dynamic_display.o src/log.o
	$(CC) $(CFLAGS) -o test/test_dynamic_display_fixed test/test_dynamic_display_fixed.c src/dynamic_display.o src/log.o $(LDFLAGS)

# Build display pipeline tests
test_display_pipeline: test/test_display_pipeline.c src/display_pipeline.o
	$(CC) $(CFLAGS) -o test/test_display_pipeline test/test_display_pipeline.c src/display_pipeline.o $(LDFLAGS)

# Build overlay dispatch tests
test_overlay_dispatch: test/test_overlay_dispatch.c src/overlay_hotkey_add_policy.o
	$(CC) $(CFLAGS) -o test/test_overlay_dispatch test/test_overlay_dispatch.c src/overlay_hotkey_add_policy.o $(LDFLAGS)

# Build hotkey grab state tests
test_hotkey_grab_state: test/test_hotkey_grab_state.c src/hotkey_grab_state.o src/app_init.o
	$(CC) $(CFLAGS) -o test/test_hotkey_grab_state test/test_hotkey_grab_state.c src/hotkey_grab_state.o src/app_init.o $(LDFLAGS)

# Build command handlers split tests
test_command_handlers_split: test/test_command_handlers_split.c
	$(CC) $(CFLAGS) -o test/test_command_handlers_split test/test_command_handlers_split.c $(LDFLAGS)

# Build command handler behavior regression tests
test_command_handlers_behavior: test/test_command_handlers_behavior.c src/command_handlers_window.o src/command_handlers_workspace.o src/command_handlers_tiling.o src/command_handlers_ui.o src/log.o
	$(CC) $(CFLAGS) -o test/test_command_handlers_behavior test/test_command_handlers_behavior.c src/command_handlers_window.o src/command_handlers_workspace.o src/command_handlers_tiling.o src/command_handlers_ui.o src/log.o $(LDFLAGS)

# Build main-split regression tests (links all non-main objects)
test_main_split_regression: test/test_main_split_regression.c $(filter-out src/main.o,$(OBJECTS))
	$(CC) $(CFLAGS) -o test/test_main_split_regression test/test_main_split_regression.c $(filter-out src/main.o,$(OBJECTS)) $(LDFLAGS)

# Build key-handler behavioral safety-net tests (TFD-270)
# (tests include key_handler.c; split modules linked explicitly)
test_key_handler_core: test/test_key_handler_core.c src/key_handler_harpoon.o src/key_handler_tabs.o
	$(CC) $(CFLAGS) -o test/test_key_handler_core test/test_key_handler_core.c src/key_handler_harpoon.o src/key_handler_tabs.o $(LDFLAGS)

test_key_handler_harpoon: test/test_key_handler_harpoon.c src/key_handler_harpoon.o src/key_handler_tabs.o
	$(CC) $(CFLAGS) -o test/test_key_handler_harpoon test/test_key_handler_harpoon.c src/key_handler_harpoon.o src/key_handler_tabs.o $(LDFLAGS)

test_key_handler_tabs: test/test_key_handler_tabs.c src/key_handler_harpoon.o src/key_handler_tabs.o
	$(CC) $(CFLAGS) -o test/test_key_handler_tabs test/test_key_handler_tabs.c src/key_handler_harpoon.o src/key_handler_tabs.o $(LDFLAGS)

# Build workspace slot cap regression tests
# (includes workspace_slots.c directly with X11/config stubs)
test_workspace_slots_cap: test/test_workspace_slots_cap.c
	$(CC) $(CFLAGS) -o test/test_workspace_slots_cap test/test_workspace_slots_cap.c $(LDFLAGS)

# Build workspace slot occlusion behavioral tests
# (includes workspace_slots.c directly with X11/config stubs)
test_workspace_slots_occlusion: test/test_workspace_slots_occlusion.c
	$(CC) $(CFLAGS) -o test/test_workspace_slots_occlusion test/test_workspace_slots_occlusion.c $(LDFLAGS)

# Build repeat-last-action behavioral tests
# (includes repeat_action.c directly with stubs)
test_repeat_action: test/test_repeat_action.c src/log.o
	$(CC) $(CFLAGS) -o test/test_repeat_action test/test_repeat_action.c src/log.o $(LDFLAGS)

# Build run-mode behavioral tests
test_run_mode: test/test_run_mode.c src/log.o
	$(CC) $(CFLAGS) -o test/test_run_mode test/test_run_mode.c src/log.o $(LDFLAGS)

# Build command mode targeting tests
test_command_mode_targeting: test/test_command_mode_targeting.c src/log.o
	$(CC) $(CFLAGS) -o test/test_command_mode_targeting test/test_command_mode_targeting.c src/log.o $(LDFLAGS)

# Build CLI run-flag parsing tests
test_cli_args_run: test/test_cli_args_run.c src/cli_args.o src/config.o src/log.o src/utils.o
	$(CC) $(CFLAGS) -o test/test_cli_args_run test/test_cli_args_run.c src/cli_args.o src/config.o src/log.o src/utils.o $(LDFLAGS)

# Build CLI delegate-flag parsing tests
test_cli_args_delegate: test/test_cli_args_delegate.c src/cli_args.o src/config.o src/log.o src/utils.o src/daemon_socket.o
	$(CC) $(CFLAGS) -o test/test_cli_args_delegate test/test_cli_args_delegate.c src/cli_args.o src/config.o src/log.o src/utils.o src/daemon_socket.o $(LDFLAGS)

# Build daemon socket protocol/lifecycle tests
test_daemon_socket: test/test_daemon_socket.c src/daemon_socket.o src/log.o
	$(CC) $(CFLAGS) -o test/test_daemon_socket test/test_daemon_socket.c src/daemon_socket.o src/log.o $(LDFLAGS)

# Build daemon socket dispatch behavioral tests
test_daemon_socket_dispatch: test/test_daemon_socket_dispatch.c src/daemon_socket.o src/log.o
	$(CC) $(CFLAGS) -o test/test_daemon_socket_dispatch test/test_daemon_socket_dispatch.c src/daemon_socket.o src/log.o $(LDFLAGS)

# Build tab visibility safety-net tests
test_tab_visibility: test/test_tab_visibility.c src/daemon_socket.o src/log.o
	$(CC) $(CFLAGS) -o test/test_tab_visibility test/test_tab_visibility.c src/daemon_socket.o src/log.o $(LDFLAGS)

# Build command-mode candidate strip tests
test_command_candidates: test/test_command_candidates.c
	$(CC) $(CFLAGS) -o test/test_command_candidates test/test_command_candidates.c $(LDFLAGS)

# Build filter ranking behavioral tests
# (includes filter.c directly with stubs; reproduces workspace-bonus ranking bug)
test_filter_ranking: test/test_filter_ranking.c src/fzf_algo.o src/log.o
	$(CC) $(CFLAGS) -o test/test_filter_ranking test/test_filter_ranking.c src/fzf_algo.o src/log.o $(LDFLAGS)

# Build apps tab behavioral tests
# (includes apps.c directly; tests filter/sort logic with synthetic data, not GIO launch)
test_apps: test/test_apps.c src/match.o src/log.o src/system_actions.o
	$(CC) $(CFLAGS) -o test/test_apps test/test_apps.c src/match.o src/log.o src/system_actions.o $(LDFLAGS)

# Build system actions tests
# (tests load semantics and deterministic metadata for logind-backed actions)
test_system_actions: test/test_system_actions.c src/system_actions.o src/log.o
	$(CC) $(CFLAGS) -o test/test_system_actions test/test_system_actions.c src/system_actions.o src/log.o $(LDFLAGS)

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
