# Makefile for cofi - C/GTK window switcher

CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -g -Wno-deprecated-declarations -MMD -MP $(shell pkg-config --cflags gtk+-3.0 x11 gio-2.0 json-glib-1.0)
CXXFLAGS = $(CFLAGS) -std=c++11 -Iinclude
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 x11 gio-2.0 json-glib-1.0) -lm -lXrandr -lXfixes -lXft -lXrender -lstdc++

# Build number from environment (GitHub Actions) or default to 0
BUILD_NUMBER ?= 0
CFLAGS += -DBUILD_NUMBER=$(BUILD_NUMBER)

# Debug-only PrintScr observer. Lets debug builds notice PrintScr even when a
# desktop screenshot tool owns the normal key grab.
XI_DEBUG_CFLAGS = -DCOFI_DEBUG_PRINTSCR_CAPTURE $(shell pkg-config --cflags xi)
XI_DEBUG_LDFLAGS = $(shell pkg-config --libs xi)
DEBUG_PRINTSCR_CAPTURE ?= 0
ifeq ($(DEBUG_PRINTSCR_CAPTURE),1)
CFLAGS += $(XI_DEBUG_CFLAGS)
LDFLAGS += $(XI_DEBUG_LDFLAGS)
endif

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
          src/slot_store.c \
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
          src/tab_metadata.c \
          src/tab_header.c \
          src/key_handler.c \
          src/nav_keys.c \
          src/prefix_tabs.c \
          src/key_handler_harpoon.c \
          src/harpoon_provider.c \
          src/workspaces_provider.c \
          src/window_lifecycle.c \
          src/hotkey_dispatch.c \
          src/command_mode.c \
          src/run_mode.c \
          src/apps.c \
          src/sinks.c \
          src/proc.c \
          src/process_windows.c \
          src/sessions.c \
          src/sessions_refresh.c \
          src/sessions_parse.c \
          src/sessions_commands.c \
          src/sessions_folder_windows.c \
          src/sessions_window_env.c \
          src/sessions_tmux_windows.c \
          src/sessions_zellij_windows.c \
          src/system_actions.c \
          src/path_binaries.c \
          src/detach_launch.c \
          src/command_handlers.c \
          src/command_handlers_window.c \
          src/command_handlers_workspace.c \
          src/command_handlers_tiling.c \
          src/command_handlers_ui.c \
          src/command_registry.c \
          src/command_availability.c \
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
          src/overlay_rules.c \
          src/overlay_sessions.c \
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
          src/rules_replay.c \
          src/display_pipeline.c \
          src/repeat_action.c \
          src/daemon_socket.c \
          src/daemon_socket_runtime.c \
          src/browser_profiles.c \
          src/calc.c \
          src/calc_provider.c \
          src/config_provider.c \
          src/apps_provider.c \
          src/hotkeys_provider.c \
          src/names_provider.c \
          src/rules_provider.c \
          src/sinks_provider.c \
          src/run_provider.c \
          src/proc_provider.c \
          src/sessions_provider.c \
          src/profiles_provider.c \
          src/cofi_modal.c \
          src/tinyexpr.c \
          src/cofi_tab_provider.c

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

# Release build used by copied installs. This intentionally rebuilds without
# debug-only flags so a previous `make debug` cannot leak into `make install`.
.PHONY: release
release:
	$(MAKE) clean
	$(MAKE) DEBUG_PRINTSCR_CAPTURE=0 $(TARGET)

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

# Install systemd user service
.PHONY: install-service
install-service:
	install -d $(SYSTEMD_USER_DIR)
	sed "s|@BINDIR@|$(BINDIR)|g" scripts/cofi.service > $(SYSTEMD_USER_DIR)/cofi.service
	systemctl --user daemon-reload
	systemctl --user reenable cofi
	systemctl --user restart cofi

# Install copied binary + systemd user service (release mode)
.PHONY: install
install: release
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/
	$(MAKE) install-service PREFIX="$(PREFIX)"
	@echo "Installed to $(BINDIR)/cofi and enabled systemd user service"

# Install symlinked binary + systemd user service (development mode)
.PHONY: install-dev
install-dev: $(TARGET)
	install -d $(BINDIR)
	ln -sf $(CURDIR)/$(TARGET) $(BINDIR)/$(TARGET)
	$(MAKE) install-service PREFIX="$(PREFIX)"
	@echo "Installed dev symlink $(BINDIR)/cofi -> $(CURDIR)/$(TARGET) and enabled systemd user service"

# Uninstall binary + systemd service
.PHONY: uninstall
uninstall:
	-systemctl --user disable --now cofi
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(SYSTEMD_USER_DIR)/cofi.service
	systemctl --user daemon-reload
	@echo "Uninstalled cofi"

# Debug build with debug output enabled
debug: CFLAGS += -DDEBUG $(XI_DEBUG_CFLAGS)
debug: LDFLAGS += $(XI_DEBUG_LDFLAGS)
debug: clean $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Test targets
test: test_window_matcher test_command_parsing test_command_parser_execution test_config_roundtrip test_config_set test_hotkey_config test_fzf_algo test_named_window test_match_scoring test_command_aliases test_wildcard_match test_parse_shortcut test_scrollbar test_rules test_rules_replay test_command_dispatch test_dynamic_display_fixed test_display_pipeline test_overlay_dispatch test_overlay_delete_flow test_overlay_rules test_hotkey_grab_state test_hotkey_rebind_flow test_command_handlers_split test_command_handlers_behavior test_main_split_regression test_key_handler_core test_key_handler_harpoon test_key_handler_tabs test_nav_keys test_workspace_slots_cap test_workspace_slots_occlusion test_window_lifecycle_fixed_reset test_initial_slot_overlays test_repeat_action test_run_mode test_cli_args_run test_filter_ranking test_apps test_apps_provider test_config_provider test_harpoon_provider test_workspaces_provider test_hotkeys_provider test_names_provider test_rules_provider test_browser_profiles test_profiles_provider test_sinks test_sinks_provider test_proc test_proc_provider test_sessions test_sessions_provider test_slot_store test_system_actions test_path_binaries test_command_mode_targeting test_daemon_socket test_daemon_socket_dispatch test_cli_args_delegate test_tab_visibility test_tab_header test_tab_metadata test_command_candidates test_detach_launch test/test_detach_survival_bin test_calc test_calc_provider test_cofi_tab_provider test_cofi_modal test_run_provider
	cd test && ./run_tests.sh

.PHONY: test-integration
test-integration: $(TARGET)
	@test/integration/run_all.sh

# Build command parsing test
test_command_parsing: test/test_command_parsing.c test/command_handler_stubs.c src/command_parser.o src/command_registry.o src/cofi_tab_provider.o
	$(CC) $(CFLAGS) -o test/test_command_parsing test/test_command_parsing.c test/command_handler_stubs.c src/command_parser.o src/command_registry.o src/cofi_tab_provider.o $(LDFLAGS)

# Build command parser execution-path test
test_command_parser_execution: test/test_command_parser_execution.c test/command_handler_stubs.c src/command_parser.o src/command_registry.o src/cofi_tab_provider.o
	$(CC) $(CFLAGS) -o test/test_command_parser_execution test/test_command_parser_execution.c test/command_handler_stubs.c src/command_parser.o src/command_registry.o src/cofi_tab_provider.o $(LDFLAGS)

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
test_command_aliases: test/test_command_aliases.c test/command_handler_stubs.c src/command_parser.o src/command_registry.o src/cofi_tab_provider.o
	$(CC) $(CFLAGS) -o test/test_command_aliases test/test_command_aliases.c test/command_handler_stubs.c src/command_parser.o src/command_registry.o src/cofi_tab_provider.o $(LDFLAGS)

# Build wildcard match test
test_wildcard_match: test/test_wildcard_match.c src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_wildcard_match test/test_wildcard_match.c src/window_matcher.o src/log.o $(LDFLAGS)

# Build parse shortcut test
test_parse_shortcut: test/test_parse_shortcut.c src/utils.o
	$(CC) $(CFLAGS) -o test/test_parse_shortcut test/test_parse_shortcut.c src/utils.o $(LDFLAGS)

# Build command dispatch test
test_command_dispatch: test/test_command_dispatch.c src/command_parser.o src/command_registry.o src/cofi_tab_provider.o
	$(CC) $(CFLAGS) -DCOMMAND_POLICY_ONLY -o test/test_command_dispatch test/test_command_dispatch.c src/command_parser.o src/command_registry.o src/cofi_tab_provider.o src/command_handlers.c $(LDFLAGS)

# Build rules test
test_rules: test/test_rules.c src/rules_config.o src/rules.o src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_rules test/test_rules.c src/rules_config.o src/rules.o src/window_matcher.o src/log.o $(LDFLAGS)

# Build rules replay test
# (tests stateless replay executor over currently open windows)
test_rules_replay: test/test_rules_replay.c src/rules_replay.o src/window_matcher.o
	$(CC) $(CFLAGS) -o test/test_rules_replay test/test_rules_replay.c src/rules_replay.o src/window_matcher.o $(LDFLAGS)

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

# Build overlay delete-flow behavior tests
# (tests harpoon delete confirm/cancel lifecycle with stubs)
test_overlay_delete_flow: test/test_overlay_delete_flow.c src/overlay_harpoon.o src/overlay_name.o
	$(CC) $(CFLAGS) -o test/test_overlay_delete_flow test/test_overlay_delete_flow.c src/overlay_harpoon.o src/overlay_name.o $(LDFLAGS)

# Build hotkey rebind flow behavioral tests
# Calls production rebind helpers (apply_rebind, show_rebind_conflict,
# handle_rebind_confirm_key) directly. Uses ld --wrap to intercept collaborators
# (hide_overlay, save_hotkey_config, regrab_hotkeys, filter_hotkeys, update_display,
# validate_selection, update_scroll_position, log_log) so production code runs but
# side effects are observed via test stubs.
HOTKEY_REBIND_WRAP = \
	-Wl,--wrap=hide_overlay \
	-Wl,--wrap=save_hotkey_config \
	-Wl,--wrap=regrab_hotkeys \
	-Wl,--wrap=filter_hotkeys \
	-Wl,--wrap=hotkeys_select_key \
	-Wl,--wrap=update_display \
	-Wl,--wrap=validate_selection \
	-Wl,--wrap=update_scroll_position \
	-Wl,--wrap=log_log \
	-Wl,--wrap=gtk_entry_get_text

test_hotkey_rebind_flow: test/test_hotkey_rebind_flow.c src/overlay_hotkey_add.o src/overlay_hotkey_add_policy.o src/hotkey_config.o
	$(CC) $(CFLAGS) -o test/test_hotkey_rebind_flow test/test_hotkey_rebind_flow.c src/overlay_hotkey_add.o src/overlay_hotkey_add_policy.o src/hotkey_config.o $(HOTKEY_REBIND_WRAP) $(LDFLAGS)

# Build rules overlay behavior tests
# (tests rules CRUD persistence-only behavior and clamp)
test_overlay_rules: test/test_overlay_rules.c test/command_handler_stubs.c src/overlay_rules.o src/command_parser.o src/command_registry.o src/cofi_tab_provider.o
	$(CC) $(CFLAGS) -o test/test_overlay_rules test/test_overlay_rules.c test/command_handler_stubs.c src/overlay_rules.o src/command_parser.o src/command_registry.o src/cofi_tab_provider.o $(LDFLAGS)

# Build hotkey grab state tests
test_hotkey_grab_state: test/test_hotkey_grab_state.c src/hotkey_grab_state.o src/app_init.o
	$(CC) $(CFLAGS) -o test/test_hotkey_grab_state test/test_hotkey_grab_state.c src/hotkey_grab_state.o src/app_init.o $(LDFLAGS)

# Build command handlers split tests
test_command_handlers_split: test/test_command_handlers_split.c src/command_registry.o
	$(CC) $(CFLAGS) -o test/test_command_handlers_split test/test_command_handlers_split.c src/command_registry.o $(LDFLAGS)

# Build command handler behavior regression tests
test_command_handlers_behavior: test/test_command_handlers_behavior.c src/command_handlers_window.o src/command_handlers_workspace.o src/command_handlers_tiling.o src/command_handlers_ui.o src/command_registry.o src/command_availability.o src/slot_store.o src/log.o
	$(CC) $(CFLAGS) -o test/test_command_handlers_behavior test/test_command_handlers_behavior.c src/command_handlers_window.o src/command_handlers_workspace.o src/command_handlers_tiling.o src/command_handlers_ui.o src/command_registry.o src/command_availability.o src/slot_store.o src/log.o $(LDFLAGS)

# Build proc parser/behavior tests
test_proc: test/test_proc.c
	$(CC) $(CFLAGS) -o test/test_proc test/test_proc.c $(LDFLAGS)

# Build main-split regression tests (links all non-main objects)
test_main_split_regression: test/test_main_split_regression.c $(filter-out src/main.o,$(OBJECTS))
	$(CC) $(CFLAGS) -o test/test_main_split_regression test/test_main_split_regression.c $(filter-out src/main.o,$(OBJECTS)) $(LDFLAGS)

# Build key-handler behavioral safety-net tests (TFD-270)
# (tests include key_handler.c; split modules linked explicitly)
test_key_handler_core: test/test_key_handler_core.c test/test_sessions_key_stubs.c src/key_handler_harpoon.o src/prefix_tabs.o src/slot_store.o src/calc.o src/tinyexpr.o src/nav_keys.o src/sessions_parse.o
	$(CC) $(CFLAGS) -o test/test_key_handler_core test/test_key_handler_core.c test/test_sessions_key_stubs.c src/key_handler_harpoon.o src/prefix_tabs.o src/slot_store.o src/calc.o src/tinyexpr.o src/nav_keys.o src/sessions_parse.o $(LDFLAGS)

test_key_handler_harpoon: test/test_key_handler_harpoon.c test/test_sessions_key_stubs.c src/key_handler_harpoon.o src/prefix_tabs.o src/slot_store.o src/calc.o src/tinyexpr.o src/nav_keys.o src/sessions_parse.o
	$(CC) $(CFLAGS) -o test/test_key_handler_harpoon test/test_key_handler_harpoon.c test/test_sessions_key_stubs.c src/key_handler_harpoon.o src/prefix_tabs.o src/slot_store.o src/calc.o src/tinyexpr.o src/nav_keys.o src/sessions_parse.o $(LDFLAGS)

test_key_handler_tabs: test/test_key_handler_tabs.c src/key_handler_harpoon.o src/harpoon_provider.o src/config_provider.o src/hotkeys_provider.o src/names_provider.o src/rules_provider.o src/prefix_tabs.o src/slot_store.o src/calc.o src/tinyexpr.o src/nav_keys.o src/sessions_parse.o
	$(CC) $(CFLAGS) -o test/test_key_handler_tabs test/test_key_handler_tabs.c src/key_handler_harpoon.o src/harpoon_provider.o src/config_provider.o src/hotkeys_provider.o src/names_provider.o src/rules_provider.o src/prefix_tabs.o src/slot_store.o src/calc.o src/tinyexpr.o src/nav_keys.o src/sessions_parse.o $(LDFLAGS)

test_nav_keys: test/test_nav_keys.c src/nav_keys.o
	$(CC) $(CFLAGS) -o test/test_nav_keys test/test_nav_keys.c src/nav_keys.o $(LDFLAGS)

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
test_run_mode: test/test_run_mode.c src/log.o src/detach_launch.o
	$(CC) $(CFLAGS) -o test/test_run_mode test/test_run_mode.c src/log.o src/detach_launch.o $(LDFLAGS)

# Build detach-launch terminal detection tests
# Note: detach_launch.c compiled inline with -DCOFI_TESTING to expose test hook
test_detach_launch: test/test_detach_launch.c src/detach_launch.c src/log.o
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_detach_launch test/test_detach_launch.c src/detach_launch.c src/log.o $(LDFLAGS)

# Build process-group survival test binary (tests fork+setsid+double-fork)
test/test_detach_survival_bin: test/test_detach_survival_bin.c
	$(CC) -o $@ $<

# Build command mode targeting tests
test_command_mode_targeting: test/test_command_mode_targeting.c test/command_handler_stubs.c src/command_registry.o src/log.o src/nav_keys.o
	$(CC) $(CFLAGS) -o test/test_command_mode_targeting test/test_command_mode_targeting.c test/command_handler_stubs.c src/command_registry.o src/log.o src/nav_keys.o $(LDFLAGS)

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
test_tab_visibility: test/test_tab_visibility.c src/daemon_socket.o src/slot_store.o src/log.o src/tab_metadata.o src/command_availability.o src/command_registry.o
	$(CC) $(CFLAGS) -o test/test_tab_visibility test/test_tab_visibility.c src/daemon_socket.o src/slot_store.o src/log.o src/tab_metadata.o src/command_availability.o src/command_registry.o $(LDFLAGS)

# Build tab header overflow tests
test_tab_header: test/test_tab_header.c src/tab_metadata.o
	$(CC) $(CFLAGS) -o test/test_tab_header test/test_tab_header.c src/tab_metadata.o $(LDFLAGS)

# Build tab metadata exhaustiveness tests
test_tab_metadata: test/test_tab_metadata.c src/tab_metadata.o
	$(CC) $(CFLAGS) -o test/test_tab_metadata test/test_tab_metadata.c src/tab_metadata.o $(LDFLAGS)

# Build command-mode candidate strip tests
test_command_candidates: test/test_command_candidates.c test/command_handler_stubs.c src/cofi_tab_provider.o src/command_availability.o src/command_registry.o src/nav_keys.o src/tab_metadata.o src/tab_header.o
	$(CC) $(CFLAGS) -o test/test_command_candidates test/test_command_candidates.c test/command_handler_stubs.c src/cofi_tab_provider.o src/command_availability.o src/command_registry.o src/nav_keys.o src/tab_metadata.o src/tab_header.o $(LDFLAGS)

# Build filter ranking behavioral tests
# (includes filter.c directly with stubs; reproduces workspace-bonus ranking bug)
test_filter_ranking: test/test_filter_ranking.c src/fzf_algo.o src/log.o
	$(CC) $(CFLAGS) -o test/test_filter_ranking test/test_filter_ranking.c src/fzf_algo.o src/log.o $(LDFLAGS)

# Build apps tab behavioral tests
# (includes apps.c directly; tests filter/sort logic with synthetic data, not GIO launch)
test_apps: test/test_apps.c src/match.o src/log.o src/system_actions.o src/detach_launch.o
	$(CC) $(CFLAGS) -o test/test_apps test/test_apps.c src/match.o src/log.o src/system_actions.o src/detach_launch.o $(LDFLAGS)

# Build Apps provider wiring tests
test_apps_provider: test/test_apps_provider.c
	$(CC) $(CFLAGS) -o test/test_apps_provider test/test_apps_provider.c $(LDFLAGS)

test_config_provider: test/test_config_provider.c
	$(CC) $(CFLAGS) -o test/test_config_provider test/test_config_provider.c $(LDFLAGS)

test_harpoon_provider: test/test_harpoon_provider.c
	$(CC) $(CFLAGS) -o test/test_harpoon_provider test/test_harpoon_provider.c $(LDFLAGS)

test_workspaces_provider: test/test_workspaces_provider.c
	$(CC) $(CFLAGS) -o test/test_workspaces_provider test/test_workspaces_provider.c $(LDFLAGS)

test_browser_profiles: test/test_browser_profiles.c src/browser_profiles.c src/fzf_algo.o src/log.o
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_browser_profiles test/test_browser_profiles.c src/browser_profiles.c src/fzf_algo.o src/log.o $(LDFLAGS)

test_profiles_provider: test/test_profiles_provider.c src/fzf_algo.o src/slot_store.o
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_profiles_provider test/test_profiles_provider.c src/fzf_algo.o src/slot_store.o $(LDFLAGS)

test_hotkeys_provider: test/test_hotkeys_provider.c
	$(CC) $(CFLAGS) -o test/test_hotkeys_provider test/test_hotkeys_provider.c $(LDFLAGS)

test_names_provider: test/test_names_provider.c
	$(CC) $(CFLAGS) -o test/test_names_provider test/test_names_provider.c $(LDFLAGS)

test_rules_provider: test/test_rules_provider.c
	$(CC) $(CFLAGS) -o test/test_rules_provider test/test_rules_provider.c $(LDFLAGS)

# Build sinks tab parser tests
test_sinks: test/test_sinks.c
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_sinks test/test_sinks.c $(LDFLAGS)

test_sinks_provider: test/test_sinks_provider.c
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_sinks_provider test/test_sinks_provider.c $(LDFLAGS)

test_proc_provider: test/test_proc_provider.c
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_proc_provider test/test_proc_provider.c $(LDFLAGS)

# Build sessions tab parser and command tests
test_sessions: test/test_sessions.c src/sessions_parse.o src/sessions_commands.o src/sessions_folder_windows.o src/sessions_window_env.c src/sessions_window_env.h src/sessions_tmux_windows.c src/sessions_tmux_windows.h src/sessions_zellij_windows.c src/sessions_zellij_windows.h
	$(CC) $(CFLAGS) -o test/test_sessions test/test_sessions.c src/sessions_parse.o src/sessions_commands.o src/sessions_folder_windows.o $(LDFLAGS)

test_sessions_provider: test/test_sessions_provider.c
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_sessions_provider test/test_sessions_provider.c $(LDFLAGS)

# Build PATH binaries tests
# (tests async-path cache dedupe/filtering, monitor hooks, and $-routing in Apps tab)
# Note: path_binaries.c compiled inline with -DCOFI_TESTING to expose test hooks
test_path_binaries: test/test_path_binaries.c src/path_binaries.c src/match.o src/log.o src/tab_metadata.o
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_path_binaries test/test_path_binaries.c src/path_binaries.c src/match.o src/log.o src/tab_metadata.o $(LDFLAGS)

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
test_slot_store: test/test_slot_store.c src/slot_store.o src/log.o
	$(CC) $(CFLAGS) -o test/test_slot_store test/test_slot_store.c src/slot_store.o src/log.o $(LDFLAGS)

test_window_matcher: test/test_window_matcher.c src/window_matcher.o src/log.o
	$(CC) $(CFLAGS) -o test/test_window_matcher test/test_window_matcher.c src/window_matcher.o src/log.o $(LDFLAGS)

test_harpoon_integration: test/test_harpoon_integration.c src/harpoon.o src/slot_store.o src/window_matcher.o src/log.o src/utils.o
	$(CC) $(CFLAGS) -o test/test_harpoon_integration test/test_harpoon_integration.c src/harpoon.o src/slot_store.o src/window_matcher.o src/log.o src/utils.o $(LDFLAGS)

test_event_sequence: test/test_event_sequence.c src/harpoon.o src/slot_store.o src/window_matcher.o src/log.o src/utils.o
	$(CC) $(CFLAGS) -o test/test_event_sequence test/test_event_sequence.c src/harpoon.o src/slot_store.o src/window_matcher.o src/log.o src/utils.o $(LDFLAGS)

test_calc: test/test_calc.c src/tinyexpr.o src/log.o
	$(CC) $(CFLAGS) -o test/test_calc test/test_calc.c src/tinyexpr.o src/log.o $(LDFLAGS)

test_calc_provider: test/test_calc_provider.c src/calc.o src/tinyexpr.o src/cofi_tab_provider.o src/log.o
	$(CC) $(CFLAGS) -o test/test_calc_provider test/test_calc_provider.c src/calc.o src/tinyexpr.o src/cofi_tab_provider.o src/log.o $(LDFLAGS)

test_cofi_tab_provider: test/test_cofi_tab_provider.c src/cofi_tab_provider.c
	$(CC) $(CFLAGS) -o test/test_cofi_tab_provider test/test_cofi_tab_provider.c src/cofi_tab_provider.c $(LDFLAGS)

test_cofi_modal: test/test_cofi_modal.c src/cofi_modal.c src/cofi_tab_provider.c
	$(CC) $(CFLAGS) -o test/test_cofi_modal test/test_cofi_modal.c src/cofi_modal.c src/cofi_tab_provider.c $(LDFLAGS)

test_run_provider: test/test_run_provider.c src/tab_metadata.o
	$(CC) $(CFLAGS) -o test/test_run_provider test/test_run_provider.c src/tab_metadata.o $(LDFLAGS)

test_window_lifecycle_fixed_reset: test/test_window_lifecycle_fixed_reset.c
	$(CC) $(CFLAGS) -o test/test_window_lifecycle_fixed_reset test/test_window_lifecycle_fixed_reset.c $(LDFLAGS)

test_initial_slot_overlays: test/test_initial_slot_overlays.c
	$(CC) $(CFLAGS) -o test/test_initial_slot_overlays test/test_initial_slot_overlays.c $(LDFLAGS)


clean_tests:
	rm -f test/test_* test/*.o

-include $(wildcard src/*.d)

.PHONY: all clean install uninstall debug run test test-integration build_tests clean_tests
