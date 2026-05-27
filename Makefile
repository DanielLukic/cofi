# Makefile for cofi - C/GTK window switcher

CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -g -Wno-deprecated-declarations -MMD -MP -Isrc $(shell pkg-config --cflags gtk+-3.0 x11 gio-2.0 json-glib-1.0)
CXXFLAGS = $(CFLAGS) -std=c++11 -Iinclude
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 x11 gio-2.0 json-glib-1.0) -lm -lXrandr -lXfixes -lXft -lXrender -lstdc++

# Build number from environment (GitHub Actions) or default to 0
BUILD_NUMBER ?= 0
CFLAGS += -DBUILD_NUMBER=$(BUILD_NUMBER)

# Git short hash for dev/local builds; empty string in environments with no git
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null)
GIT_DIRTY := $(if $(shell git status --porcelain 2>/dev/null),-dirty)
ifneq ($(GIT_HASH),)
CFLAGS += -DCOFI_GIT_HASH='"$(GIT_HASH)$(GIT_DIRTY)"'
endif

# Debug-only PrintScr observer. Lets debug builds notice PrintScr even when a
# desktop screenshot tool owns the normal key grab.
XI_DEBUG_CFLAGS = -DCOFI_DEBUG_PRINTSCR_CAPTURE $(shell pkg-config --cflags xi)
XI_DEBUG_LDFLAGS = $(shell pkg-config --libs xi)
DEBUG_PRINTSCR_CAPTURE ?= 0
ifeq ($(DEBUG_PRINTSCR_CAPTURE),1)
CFLAGS += $(XI_DEBUG_CFLAGS)
LDFLAGS += $(XI_DEBUG_LDFLAGS)
endif

# Source files — recursive so per-feature subfolders are picked up automatically.
SOURCES = $(shell find src -type f \( -name '*.c' -o -name '*.cpp' \) | sort)

# Separate C and C++ sources
C_SOURCES = $(filter %.c,$(SOURCES))
CPP_SOURCES = $(filter %.cpp,$(SOURCES))

# Object files
C_OBJECTS = $(C_SOURCES:.c=.o)
CPP_OBJECTS = $(CPP_SOURCES:.cpp=.o)
OBJECTS = $(C_OBJECTS) $(CPP_OBJECTS)

# Centralized source object paths. Move commits should update these variables,
# not every test rule that links a production object.
src_obj = src/$(1).o
app_init_obj = src/core/app/app_init.o
calc_obj = src/calc/calc.o
cli_args_obj = src/cli/cli_args.o
cofi_json_io_obj = src/core/json/cofi_json_io.o
builtin_plugins_obj = src/providers/builtin_plugins.o
cofi_tab_provider_obj = src/providers/cofi_tab_provider.o
command_availability_obj = src/commands/command_availability.o
command_handlers_obj = src/commands/command_handlers.o
command_handlers_tiling_obj = src/commands/command_handlers_tiling.o
command_handlers_ui_obj = src/commands/command_handlers_ui.o
command_handlers_window_obj = src/commands/command_handlers_window.o
command_handlers_workspace_obj = src/commands/command_handlers_workspace.o
command_parser_obj = src/commands/command_parser.o
command_registry_obj = src/commands/command_registry.o
config_obj = src/config/config.o
config_provider_obj = src/config/config_provider.o
core_commands_obj = src/commands/core_commands.o
daemon_socket_obj = src/daemon/daemon_socket.o
daemon_socket_runtime_obj = src/daemon/daemon_socket_runtime.o
detach_launch_obj = src/daemon/detach_launch.o
cofi_modal_obj = src/ui/cofi_modal.o
display_obj = src/ui/display.o
display_pipeline_obj = src/ui/display_pipeline.o
dynamic_display_obj = src/ui/dynamic_display.o
emoji_data_obj = src/emoji/emoji_data.o
fzf_algo_obj = src/matching/fzf_algo.o
geom_provider_obj = src/geom/geom_provider.o
geom_rule_sync_obj = src/geom/geom_rule_sync.o
geometry_planner_obj = src/geom/geometry_planner.o
gtk_utils_obj = src/ui/gtk_utils.o
gtk_window_obj = src/ui/gtk_window.o
harpoon_obj = src/harpoon/harpoon.o
harpoon_config_obj = src/harpoon/harpoon_config.o
harpoon_provider_obj = src/harpoon/harpoon_provider.o
hotkey_config_obj = src/daemon/hotkey_config.o
hotkey_dispatch_obj = src/daemon/hotkey_dispatch.o
hotkey_grab_state_obj = src/daemon/hotkey_grab_state.o
hotkeys_obj = src/daemon/hotkeys.o
hotkeys_provider_obj = src/daemon/hotkeys_provider.o
key_handler_harpoon_obj = src/harpoon/key_handler_harpoon.o
layout_store_obj = src/geom/layout_store.o
log_obj = src/core/log/log.o
main_obj = src/core/app/main.o
main_testable_obj = src/core/app/main_testable.o
match_obj = src/matching/match.o
match_entry_obj = src/matching/match_entry.o
match_entry_config_obj = src/matching/match_entry_config.o
matching_gc_obj = src/matching/matching_gc.o
matching_provider_obj = src/matching/matching_provider.o
nav_keys_obj = src/core/nav_keys/nav_keys.o
key_handler_obj = src/ui/key_handler.o
overlay_confirm_obj = src/ui/overlay_confirm.o
overlay_harpoon_obj = src/harpoon/overlay_harpoon.o
overlay_hotkey_add_obj = src/daemon/overlay_hotkey_add.o
overlay_hotkey_add_policy_obj = src/daemon/overlay_hotkey_add_policy.o
overlay_hotkey_edit_obj = src/daemon/overlay_hotkey_edit.o
overlay_dispatch_obj = src/ui/overlay_dispatch.o
overlay_manager_obj = src/ui/overlay_manager.o
overlay_name_obj = src/matching/overlay_name.o
overlay_pattern_obj = src/matching/overlay_pattern.o
overlay_rules_obj = src/rules/overlay_rules.o
overlay_sessions_obj = src/sessions/overlay_sessions.o
prefix_tabs_obj = src/ui/prefix_tabs.o
overlay_projects_obj = src/projects/overlay_projects.o
path_binaries_obj = src/projects/path_binaries.o
projects_obj = src/projects/projects.o
projects_commands_obj = src/projects/projects_commands.o
projects_exec_obj = src/projects/projects_exec.o
projects_folder_windows_obj = src/projects/projects_folder_windows.o
projects_parse_obj = src/projects/projects_parse.o
projects_provider_obj = src/projects/projects_provider.o
projects_refresh_obj = src/projects/projects_refresh.o
projects_remote_scope_obj = src/projects/projects_remote_scope.o
projects_remote_store_obj = src/projects/projects_remote_store.o
projects_remote_windows_obj = src/projects/projects_remote_windows.o
projects_tmux_windows_obj = src/projects/projects_tmux_windows.o
projects_window_env_obj = src/projects/projects_window_env.o
projects_zellij_windows_obj = src/projects/projects_zellij_windows.o
rules_obj = src/rules/rules.o
rules_config_obj = src/rules/rules_config.o
rules_provider_obj = src/rules/rules_provider.o
rules_replay_obj = src/rules/rules_replay.o
rules_toggle_obj = src/rules/rules_toggle.o
slot_store_obj = src/core/slot_store/slot_store.o
system_actions_obj = src/system_actions/system_actions.o
tiling_obj = src/geom/tiling.o
tiling_overlay_obj = src/geom/tiling_overlay.o
slot_overlay_obj = src/ui/slot_overlay.o
tab_header_obj = src/ui/tab_header.o
tab_metadata_obj = src/ui/tab_metadata.o
tab_switching_obj = src/ui/tab_switching.o
tinyexpr_obj = src/core/utils/tinyexpr.o
utf8_columns_obj = src/core/utils/utf8_columns.o
utils_obj = src/core/utils/utils.o
window_display_title_obj = src/matching/window_display_title.o
window_geometry_matching_obj = src/geom/window_geometry_matching.o
window_highlight_obj = src/ui/window_highlight.o
window_lifecycle_obj = src/ui/window_lifecycle.o
window_matcher_obj = src/matching/window_matcher.o

core_app_objs = $(app_init_obj)
core_json_objs = $(cofi_json_io_obj)
core_utils_objs = $(tinyexpr_obj) $(utf8_columns_obj) $(utils_obj)
core_log_objs = $(log_obj)
core_nav_keys_objs = $(nav_keys_obj)
matching_objs = $(fzf_algo_obj) $(match_obj) $(match_entry_obj) $(match_entry_config_obj) $(matching_gc_obj) $(matching_provider_obj) $(overlay_name_obj) $(overlay_pattern_obj) $(window_display_title_obj) $(window_matcher_obj)
rules_objs = $(rules_obj) $(rules_config_obj) $(rules_provider_obj) $(rules_replay_obj) $(rules_toggle_obj) $(overlay_rules_obj)
harpoon_objs = $(harpoon_obj) $(harpoon_config_obj) $(harpoon_provider_obj) $(key_handler_harpoon_obj) $(prefix_tabs_obj)
geom_objs = $(geom_provider_obj) $(geometry_planner_obj) $(geom_rule_sync_obj) $(layout_store_obj) $(tiling_obj) $(tiling_overlay_obj) $(window_geometry_matching_obj)
daemon_objs = $(daemon_socket_obj) $(daemon_socket_runtime_obj) $(detach_launch_obj) $(hotkey_config_obj) $(hotkey_dispatch_obj) $(hotkey_grab_state_obj) $(hotkeys_obj) $(hotkeys_provider_obj) $(overlay_hotkey_add_obj) $(overlay_hotkey_add_policy_obj) $(overlay_hotkey_edit_obj)
ui_objs = $(cofi_modal_obj) $(display_obj) $(display_pipeline_obj) $(dynamic_display_obj) $(gtk_utils_obj) $(gtk_window_obj) $(key_handler_obj) $(overlay_confirm_obj) $(overlay_dispatch_obj) $(overlay_manager_obj) $(prefix_tabs_obj) $(slot_overlay_obj) $(tab_header_obj) $(tab_metadata_obj) $(tab_switching_obj) $(window_highlight_obj) $(window_lifecycle_obj)
commands_objs = $(command_availability_obj) $(command_handlers_obj) $(command_handlers_tiling_obj) $(command_handlers_ui_obj) $(command_handlers_window_obj) $(command_handlers_workspace_obj) $(command_parser_obj) $(command_registry_obj) $(core_commands_obj)
providers_objs = $(builtin_plugins_obj) $(cofi_tab_provider_obj)
calc_objs = $(calc_obj) $(tinyexpr_obj)
config_objs = $(config_obj) $(config_provider_obj)
projects_objs = $(overlay_projects_obj) $(path_binaries_obj) $(projects_obj) $(projects_commands_obj) $(projects_exec_obj) $(projects_folder_windows_obj) $(projects_parse_obj) $(projects_provider_obj) $(projects_refresh_obj) $(projects_remote_scope_obj) $(projects_remote_store_obj) $(projects_remote_windows_obj) $(projects_tmux_windows_obj) $(projects_window_env_obj) $(projects_zellij_windows_obj)
system_actions_objs = $(system_actions_obj)

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
$(main_testable_obj): src/core/app/main.c
	$(CC) $(CFLAGS) -Dmain=cofi_main_entry -c $< -o $@

# Compile C++ source files
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET) $(shell find src -type f -name '*.d')
	rm -f src/*.o src/*.d
	rm -f test/test_command_parsing test/test_window_matcher


PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
SYSTEMD_USER_DIR = $(HOME)/.config/systemd/user

.PHONY: check-install-path
check-install-path:
	@target="$(BINDIR)/$(TARGET)"; \
	offenders=""; \
	seen=""; \
	old_ifs="$$IFS"; \
	IFS=:; \
	for dir in $$PATH; do \
		[ -n "$$dir" ] || dir=.; \
		candidate="$$dir/$(TARGET)"; \
		[ "$$candidate" = "$$target" ] && continue; \
		[ -e "$$candidate" ] || [ -L "$$candidate" ] || continue; \
		case ":$$seen:" in *:"$$candidate":*) continue ;; esac; \
		seen="$${seen}:$$candidate"; \
		offenders="$${offenders}$${offenders:+ }$$candidate"; \
	done; \
	IFS="$$old_ifs"; \
	if [ -n "$$offenders" ] && [ "$(FORCE)" != "1" ]; then \
		echo "Refusing to install: another cofi is already on PATH:" >&2; \
		for path in $$offenders; do echo "  $$path" >&2; done; \
		echo "Remove the duplicate, choose the matching install target, or rerun with FORCE=1." >&2; \
		exit 1; \
	fi; \
	if [ -n "$$offenders" ]; then \
		echo "Warning: installing despite duplicate cofi on PATH:" >&2; \
		for path in $$offenders; do echo "  $$path" >&2; done; \
	fi

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
	$(MAKE) check-install-path PREFIX="$(PREFIX)"
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/
	$(MAKE) install-service PREFIX="$(PREFIX)"
	@echo "Installed to $(BINDIR)/cofi and enabled systemd user service"

# Install symlinked binary + systemd user service (development mode)
.PHONY: install-dev
install-dev: $(TARGET)
	$(MAKE) check-install-path PREFIX="$(PREFIX)"
	install -d $(BINDIR)
	ln -sf $(CURDIR)/$(TARGET) $(BINDIR)/$(TARGET)
	$(MAKE) install-service PREFIX="$(PREFIX)"
	@echo "Installed dev symlink $(BINDIR)/cofi -> $(CURDIR)/$(TARGET) and enabled systemd user service"

.PHONY: install-local
install-local:
	$(MAKE) install PREFIX="$(HOME)/.local"

.PHONY: install-dev-local
install-dev-local:
	$(MAKE) install-dev PREFIX="$(HOME)/.local"

# Uninstall binary + systemd service
.PHONY: uninstall
uninstall:
	-systemctl --user disable --now cofi
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(SYSTEMD_USER_DIR)/cofi.service
	systemctl --user daemon-reload
	@echo "Uninstalled cofi"

.PHONY: uninstall-local
uninstall-local:
	$(MAKE) uninstall PREFIX="$(HOME)/.local"

# Debug build with debug output enabled
debug: CFLAGS += -DDEBUG $(XI_DEBUG_CFLAGS)
debug: LDFLAGS += $(XI_DEBUG_LDFLAGS)
debug: clean $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Test targets — derived from rule names in this file; add a test_xxx: rule and it is picked up automatically.
# Excludes test_quick (dev-only scratch target) and test/test_plugin_boundaries.o (object rule, not binary).
TEST_TARGETS = $(shell grep -Eo '^test[_/][a-z0-9_.]+:' Makefile | tr -d ':' | grep -vxF 'test_quick' | grep -vxF 'test/test_plugin_boundaries.o')

TEST_BINARIES = $(filter-out test_detach_survival_bin,$(TEST_TARGETS:test/%=%)) test_detach_survival.sh

test: $(TEST_TARGETS)
	cd test && TEST_BINARIES="$(TEST_BINARIES)" ./run_tests.sh

.PHONY: test-integration
test-integration: $(TARGET)
	@test/integration/run_all.sh
	@test/integration/run_window_state.sh

# Build command parsing test
test_command_parsing: test/test_command_parsing.c test/command_handler_stubs.c $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj)
	$(CC) $(CFLAGS) -o test/test_command_parsing test/test_command_parsing.c test/command_handler_stubs.c $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj) $(LDFLAGS)

# Build command parser execution-path test
test_command_parser_execution: test/test_command_parser_execution.c test/command_handler_stubs.c $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj)
	$(CC) $(CFLAGS) -o test/test_command_parser_execution test/test_command_parser_execution.c test/command_handler_stubs.c $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj) $(LDFLAGS)

# Build config round-trip test
test_config_roundtrip: test/test_config_roundtrip.c $(config_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_config_roundtrip test/test_config_roundtrip.c $(config_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

# Build config set/display test
test_config_set: test/test_config_set.c $(config_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_config_set test/test_config_set.c $(config_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

# Build hotkey config test
test_hotkey_config: test/test_hotkey_config.c $(hotkey_config_obj) $(cofi_json_io_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_hotkey_config test/test_hotkey_config.c $(hotkey_config_obj) $(cofi_json_io_obj) $(log_obj) $(LDFLAGS)

test_hotkey_dispatch: test/test_hotkey_dispatch.c
	$(CC) $(CFLAGS) -o test/test_hotkey_dispatch test/test_hotkey_dispatch.c $(LDFLAGS)

# Build fzf algorithm test
test_fzf_algo: test/test_fzf_algo.c $(fzf_algo_obj)
	$(CC) $(CFLAGS) -o test/test_fzf_algo test/test_fzf_algo.c $(fzf_algo_obj) $(LDFLAGS)

# Build named window test
test_match_entry: test/test_match_entry.c $(match_entry_obj) $(match_entry_config_obj) $(layout_store_obj) $(window_geometry_matching_obj) $(geometry_planner_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_match_entry test/test_match_entry.c $(match_entry_obj) $(match_entry_config_obj) $(layout_store_obj) $(window_geometry_matching_obj) $(geometry_planner_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

# Build match scoring test (fzy algorithm)
test_match_scoring: test/test_match_scoring.c $(match_obj)
	$(CC) $(CFLAGS) -o test/test_match_scoring test/test_match_scoring.c $(match_obj) $(LDFLAGS)

# Build command alias edge case test
test_command_aliases: test/test_command_aliases.c test/command_handler_stubs.c $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj)
	$(CC) $(CFLAGS) -o test/test_command_aliases test/test_command_aliases.c test/command_handler_stubs.c $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj) $(LDFLAGS)

# Build wildcard match test
test_wildcard_match: test/test_wildcard_match.c $(window_matcher_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_wildcard_match test/test_wildcard_match.c $(window_matcher_obj) $(log_obj) $(LDFLAGS)

# Build parse shortcut test
test_parse_shortcut: test/test_parse_shortcut.c $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_parse_shortcut test/test_parse_shortcut.c $(utils_obj) $(LDFLAGS)

# Build command dispatch test
test_command_dispatch: test/test_command_dispatch.c test/command_handler_stubs.c $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj) $(command_availability_obj)
	$(CC) $(CFLAGS) -DCOMMAND_POLICY_ONLY -o test/test_command_dispatch test/test_command_dispatch.c test/command_handler_stubs.c $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj) $(command_availability_obj) src/commands/command_handlers.c $(LDFLAGS)

# Build rules test
test_rules: test/test_rules.c $(rules_config_obj) $(rules_obj) $(match_entry_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_rules test/test_rules.c $(rules_config_obj) $(rules_obj) $(match_entry_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

# Build rules replay test
# (tests stateless replay executor over currently open windows)
test_rules_replay: test/test_rules_replay.c $(rules_replay_obj) $(rules_obj) $(match_entry_obj) $(window_matcher_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_rules_replay test/test_rules_replay.c $(rules_replay_obj) $(rules_obj) $(match_entry_obj) $(window_matcher_obj) $(utils_obj) $(LDFLAGS)

# Build rules once/ applied-window tracking test
test_rules_once: test/test_rules_once.c $(rules_replay_obj) $(rules_obj) $(rules_toggle_obj) $(match_entry_obj) $(window_matcher_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_rules_once test/test_rules_once.c $(rules_replay_obj) $(rules_obj) $(rules_toggle_obj) $(match_entry_obj) $(window_matcher_obj) $(utils_obj) $(LDFLAGS)

# Build scrollbar overlay test (extracts scrollbar functions only)
test_scrollbar: test/test_scrollbar.c $(utf8_columns_obj)
	$(CC) $(CFLAGS) -DSCROLLBAR_TEST_STANDALONE -o test/test_scrollbar test/test_scrollbar.c $(utf8_columns_obj) $(LDFLAGS)

# Build fixed window sizing tests
test_dynamic_display_fixed: test/test_dynamic_display_fixed.c $(dynamic_display_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_dynamic_display_fixed test/test_dynamic_display_fixed.c $(dynamic_display_obj) $(log_obj) $(LDFLAGS)

# Build display pipeline tests
test_display_pipeline: test/test_display_pipeline.c $(display_pipeline_obj)
	$(CC) $(CFLAGS) -o test/test_display_pipeline test/test_display_pipeline.c $(display_pipeline_obj) $(LDFLAGS)

test_utf8_columns: test/test_utf8_columns.c $(utf8_columns_obj)
	$(CC) $(CFLAGS) -o test/test_utf8_columns test/test_utf8_columns.c $(utf8_columns_obj) $(LDFLAGS)

test_emoji_data: test/test_emoji_data.c $(emoji_data_obj)
	$(CC) $(CFLAGS) -o test/test_emoji_data test/test_emoji_data.c $(emoji_data_obj) $(LDFLAGS)

test_emoji_provider: test/test_emoji_provider.c $(emoji_data_obj) $(fzf_algo_obj) $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -o test/test_emoji_provider test/test_emoji_provider.c $(emoji_data_obj) $(fzf_algo_obj) $(cofi_json_io_obj) $(LDFLAGS)

test_geom_provider: test/test_geom_provider.c $(layout_store_obj) $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -o test/test_geom_provider test/test_geom_provider.c $(layout_store_obj) $(cofi_json_io_obj) $(LDFLAGS)

test_geom_rule_sync: test/test_geom_rule_sync.c $(geom_rule_sync_obj) $(rules_config_obj) $(match_entry_obj) $(match_entry_config_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_geom_rule_sync test/test_geom_rule_sync.c $(geom_rule_sync_obj) $(rules_config_obj) $(match_entry_obj) $(match_entry_config_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

test_provider_selection: test/test_provider_selection.c $(emoji_data_obj) $(fzf_algo_obj) $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -o test/test_provider_selection test/test_provider_selection.c $(emoji_data_obj) $(fzf_algo_obj) $(cofi_json_io_obj) $(LDFLAGS)

test_emoji_ranking: test/test_emoji_ranking.c $(emoji_data_obj) $(fzf_algo_obj) $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -o test/test_emoji_ranking test/test_emoji_ranking.c $(emoji_data_obj) $(fzf_algo_obj) $(cofi_json_io_obj) $(LDFLAGS)

test_emoji_history: test/test_emoji_history.c $(emoji_data_obj) $(fzf_algo_obj) $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -o test/test_emoji_history test/test_emoji_history.c $(emoji_data_obj) $(fzf_algo_obj) $(cofi_json_io_obj) $(LDFLAGS)

# Build overlay dispatch tests
test_overlay_dispatch: test/test_overlay_dispatch.c $(overlay_hotkey_add_policy_obj)
	$(CC) $(CFLAGS) -o test/test_overlay_dispatch test/test_overlay_dispatch.c $(overlay_hotkey_add_policy_obj) $(LDFLAGS)

# Build overlay delete-flow behavior tests
# (tests harpoon delete confirm/cancel lifecycle with stubs)
test_overlay_delete_flow: test/test_overlay_delete_flow.c $(overlay_harpoon_obj) $(overlay_name_obj) $(overlay_sessions_obj) $(overlay_confirm_obj)
	$(CC) $(CFLAGS) -o test/test_overlay_delete_flow test/test_overlay_delete_flow.c $(overlay_harpoon_obj) $(overlay_name_obj) $(overlay_sessions_obj) $(overlay_confirm_obj) $(LDFLAGS)

# Build shared confirm overlay tests
test_overlay_confirm: test/test_overlay_confirm.c $(overlay_confirm_obj) $(gtk_utils_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_overlay_confirm test/test_overlay_confirm.c $(overlay_confirm_obj) $(gtk_utils_obj) $(log_obj) $(LDFLAGS)

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

test_hotkey_rebind_flow: test/test_hotkey_rebind_flow.c $(overlay_hotkey_add_obj) $(overlay_hotkey_add_policy_obj) $(hotkey_config_obj) $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -o test/test_hotkey_rebind_flow test/test_hotkey_rebind_flow.c $(overlay_hotkey_add_obj) $(overlay_hotkey_add_policy_obj) $(hotkey_config_obj) $(cofi_json_io_obj) $(HOTKEY_REBIND_WRAP) $(LDFLAGS)

# Build rules overlay behavior tests
# (tests rules CRUD persistence-only behavior and clamp)
test_overlay_rules: test/test_overlay_rules.c test/command_handler_stubs.c $(overlay_rules_obj) $(overlay_confirm_obj) $(gtk_utils_obj) $(log_obj) $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj)
	$(CC) $(CFLAGS) -o test/test_overlay_rules test/test_overlay_rules.c test/command_handler_stubs.c $(overlay_rules_obj) $(overlay_confirm_obj) $(gtk_utils_obj) $(log_obj) $(command_parser_obj) $(core_commands_obj) $(command_registry_obj) $(cofi_tab_provider_obj) $(LDFLAGS)

# Build shared pattern overlay tests
test_overlay_pattern: test/test_overlay_pattern.c $(overlay_pattern_obj) $(overlay_manager_obj) $(gtk_utils_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_overlay_pattern test/test_overlay_pattern.c $(overlay_pattern_obj) $(overlay_manager_obj) $(gtk_utils_obj) $(log_obj) $(LDFLAGS)

# Build hotkey grab state tests
test_hotkey_grab_state: test/test_hotkey_grab_state.c $(hotkey_grab_state_obj) $(app_init_obj) $(layout_store_obj) $(match_entry_config_obj) $(cofi_tab_provider_obj) $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj)
	$(CC) $(CFLAGS) -o test/test_hotkey_grab_state test/test_hotkey_grab_state.c $(hotkey_grab_state_obj) $(app_init_obj) $(layout_store_obj) $(match_entry_config_obj) $(cofi_tab_provider_obj) $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(LDFLAGS)

# Build command handlers split tests
test_command_handlers_split: test/test_command_handlers_split.c test/command_handler_stubs.c $(core_commands_obj) $(command_registry_obj)
	$(CC) $(CFLAGS) -o test/test_command_handlers_split test/test_command_handlers_split.c test/command_handler_stubs.c $(core_commands_obj) $(command_registry_obj) $(LDFLAGS)

# Build command handler behavior regression tests
test_command_handlers_behavior: test/test_command_handlers_behavior.c $(command_handlers_window_obj) $(command_handlers_workspace_obj) $(command_handlers_tiling_obj) $(command_handlers_ui_obj) $(core_commands_obj) $(command_registry_obj) $(command_availability_obj) $(slot_store_obj) $(match_entry_obj) $(rules_config_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(utils_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_command_handlers_behavior test/test_command_handlers_behavior.c $(command_handlers_window_obj) $(command_handlers_workspace_obj) $(command_handlers_tiling_obj) $(command_handlers_ui_obj) $(core_commands_obj) $(command_registry_obj) $(command_availability_obj) $(slot_store_obj) $(match_entry_obj) $(rules_config_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(utils_obj) $(log_obj) $(LDFLAGS)

# Build proc parser/behavior tests
test_proc: test/test_proc.c
	$(CC) $(CFLAGS) -o test/test_proc test/test_proc.c $(LDFLAGS)

# Build main-split regression tests (links all non-main objects)
test_main_split_regression: test/test_main_split_regression.c $(filter-out $(main_obj),$(OBJECTS))
	$(CC) $(CFLAGS) -o test/test_main_split_regression test/test_main_split_regression.c $(filter-out $(main_obj),$(OBJECTS)) $(LDFLAGS)

# Build key-handler behavioral safety-net tests (TFD-270)
# (tests include key_handler.c; split modules linked explicitly)
test_key_handler_core: test/test_key_handler_core.c test/test_projects_key_stubs.c $(key_handler_harpoon_obj) $(prefix_tabs_obj) $(slot_store_obj) $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(nav_keys_obj) $(projects_parse_obj)
	$(CC) $(CFLAGS) -o test/test_key_handler_core test/test_key_handler_core.c test/test_projects_key_stubs.c $(key_handler_harpoon_obj) $(prefix_tabs_obj) $(slot_store_obj) $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(nav_keys_obj) $(projects_parse_obj) $(LDFLAGS)

test_key_handler_harpoon: test/test_key_handler_harpoon.c test/test_projects_key_stubs.c $(key_handler_harpoon_obj) $(prefix_tabs_obj) $(slot_store_obj) $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(nav_keys_obj) $(projects_parse_obj)
	$(CC) $(CFLAGS) -o test/test_key_handler_harpoon test/test_key_handler_harpoon.c test/test_projects_key_stubs.c $(key_handler_harpoon_obj) $(prefix_tabs_obj) $(slot_store_obj) $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(nav_keys_obj) $(projects_parse_obj) $(LDFLAGS)

test_key_handler_tabs: test/test_key_handler_tabs.c test/command_handler_stubs.c $(key_handler_harpoon_obj) $(harpoon_provider_obj) $(config_provider_obj) $(hotkeys_provider_obj) $(matching_provider_obj) $(rules_provider_obj) $(rules_toggle_obj) $(prefix_tabs_obj) $(slot_store_obj) $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(nav_keys_obj) $(projects_parse_obj) $(command_registry_obj) $(dynamic_display_obj)
	$(CC) $(CFLAGS) -o test/test_key_handler_tabs test/test_key_handler_tabs.c test/command_handler_stubs.c $(key_handler_harpoon_obj) $(harpoon_provider_obj) $(config_provider_obj) $(hotkeys_provider_obj) $(matching_provider_obj) $(rules_provider_obj) $(rules_toggle_obj) $(prefix_tabs_obj) $(slot_store_obj) $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(nav_keys_obj) $(projects_parse_obj) $(command_registry_obj) $(dynamic_display_obj) $(LDFLAGS)

test_nav_keys: test/test_nav_keys.c $(nav_keys_obj)
	$(CC) $(CFLAGS) -o test/test_nav_keys test/test_nav_keys.c $(nav_keys_obj) $(LDFLAGS)

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
test_repeat_action: test/test_repeat_action.c $(log_obj)
	$(CC) $(CFLAGS) -o test/test_repeat_action test/test_repeat_action.c $(log_obj) $(LDFLAGS)

# Build run-mode behavioral tests
test_run_mode: test/test_run_mode.c $(log_obj) $(detach_launch_obj)
	$(CC) $(CFLAGS) -o test/test_run_mode test/test_run_mode.c $(log_obj) $(detach_launch_obj) $(LDFLAGS)

# Build detach-launch terminal detection tests
# Note: detach_launch.c compiled inline with -DCOFI_TESTING to expose test hook
test_detach_launch: test/test_detach_launch.c src/daemon/detach_launch.c $(log_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_detach_launch test/test_detach_launch.c src/daemon/detach_launch.c $(log_obj) $(LDFLAGS)

# Build process-group survival test binary (tests fork+setsid+double-fork)
test/test_detach_survival_bin: test/test_detach_survival_bin.c
	$(CC) -o $@ $<

# Build command mode targeting tests
test_command_mode_targeting: test/test_command_mode_targeting.c test/command_handler_stubs.c $(command_registry_obj) $(log_obj) $(nav_keys_obj)
	$(CC) $(CFLAGS) -o test/test_command_mode_targeting test/test_command_mode_targeting.c test/command_handler_stubs.c $(command_registry_obj) $(log_obj) $(nav_keys_obj) $(LDFLAGS)

# Build CLI run-flag parsing tests
test_cli_args_run: test/test_cli_args_run.c $(cli_args_obj) $(config_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_cli_args_run test/test_cli_args_run.c $(cli_args_obj) $(config_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

# Build CLI delegate-flag parsing tests
test_cli_args_delegate: test/test_cli_args_delegate.c $(cli_args_obj) $(config_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(daemon_socket_obj)
	$(CC) $(CFLAGS) -o test/test_cli_args_delegate test/test_cli_args_delegate.c $(cli_args_obj) $(config_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(daemon_socket_obj) $(LDFLAGS)

# Build daemon socket protocol/lifecycle tests
test_daemon_socket: test/test_daemon_socket.c $(daemon_socket_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_daemon_socket test/test_daemon_socket.c $(daemon_socket_obj) $(log_obj) $(LDFLAGS)

# Build daemon socket dispatch behavioral tests
test_daemon_socket_dispatch: test/test_daemon_socket_dispatch.c $(daemon_socket_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_daemon_socket_dispatch test/test_daemon_socket_dispatch.c $(daemon_socket_obj) $(log_obj) $(LDFLAGS)

# Build tab visibility safety-net tests
test_tab_visibility: test/test_tab_visibility.c $(daemon_socket_obj) $(slot_store_obj) $(cofi_json_io_obj) $(log_obj) $(tab_metadata_obj) $(command_availability_obj) $(core_commands_obj) $(command_registry_obj) $(rules_toggle_obj)
	$(CC) $(CFLAGS) -o test/test_tab_visibility test/test_tab_visibility.c $(daemon_socket_obj) $(slot_store_obj) $(cofi_json_io_obj) $(log_obj) $(tab_metadata_obj) $(command_availability_obj) $(core_commands_obj) $(command_registry_obj) $(rules_toggle_obj) $(LDFLAGS)

# Build tab header overflow tests
test_tab_header: test/test_tab_header.c $(tab_metadata_obj)
	$(CC) $(CFLAGS) -o test/test_tab_header test/test_tab_header.c $(tab_metadata_obj) $(LDFLAGS)

# Build tab metadata exhaustiveness tests
test_tab_metadata: test/test_tab_metadata.c $(tab_metadata_obj)
	$(CC) $(CFLAGS) -o test/test_tab_metadata test/test_tab_metadata.c $(tab_metadata_obj) $(LDFLAGS)

# Build command-mode candidate strip tests
test_command_candidates: test/test_command_candidates.c test/command_handler_stubs.c $(cofi_tab_provider_obj) $(command_availability_obj) $(core_commands_obj) $(command_registry_obj) $(nav_keys_obj) $(tab_metadata_obj) $(tab_header_obj) $(utf8_columns_obj) $(window_display_title_obj) $(match_entry_obj) $(window_matcher_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_command_candidates test/test_command_candidates.c test/command_handler_stubs.c $(cofi_tab_provider_obj) $(command_availability_obj) $(core_commands_obj) $(command_registry_obj) $(nav_keys_obj) $(tab_metadata_obj) $(tab_header_obj) $(utf8_columns_obj) $(window_display_title_obj) $(match_entry_obj) $(window_matcher_obj) $(utils_obj) $(LDFLAGS)

# Build filter ranking behavioral tests
# (includes filter.c directly with stubs; reproduces workspace-bonus ranking bug)
test_filter_ranking: test/test_filter_ranking.c $(fzf_algo_obj) $(log_obj) $(window_display_title_obj)
	$(CC) $(CFLAGS) -o test/test_filter_ranking test/test_filter_ranking.c $(fzf_algo_obj) $(log_obj) $(window_display_title_obj) $(LDFLAGS)

test_initials_ranking: test/test_initials_ranking.c $(fzf_algo_obj) $(log_obj) $(window_display_title_obj)
	$(CC) $(CFLAGS) -o test/test_initials_ranking test/test_initials_ranking.c $(fzf_algo_obj) $(log_obj) $(window_display_title_obj) $(LDFLAGS)

test_ranking_corpus: test/test_ranking_corpus.c $(fzf_algo_obj) $(log_obj) $(window_display_title_obj)
	$(CC) $(CFLAGS) -o test/test_ranking_corpus test/test_ranking_corpus.c $(fzf_algo_obj) $(log_obj) $(window_display_title_obj) $(LDFLAGS)

test_filter_title_compose: test/test_filter_title_compose.c $(window_display_title_obj) $(match_entry_obj) $(window_matcher_obj) $(fzf_algo_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_filter_title_compose test/test_filter_title_compose.c $(window_display_title_obj) $(match_entry_obj) $(window_matcher_obj) $(fzf_algo_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

# Build apps tab behavioral tests
# (includes apps.c directly; tests filter/sort logic with synthetic data, not GIO launch)
test_apps: test/test_apps.c $(match_obj) $(log_obj) $(system_actions_obj) $(detach_launch_obj)
	$(CC) $(CFLAGS) -o test/test_apps test/test_apps.c $(match_obj) $(log_obj) $(system_actions_obj) $(detach_launch_obj) $(LDFLAGS)

# Build Apps provider wiring tests
test_apps_provider: test/test_apps_provider.c
	$(CC) $(CFLAGS) -o test/test_apps_provider test/test_apps_provider.c $(LDFLAGS)

test_config_provider: test/test_config_provider.c
	$(CC) $(CFLAGS) -o test/test_config_provider test/test_config_provider.c $(LDFLAGS)

test_harpoon_provider: test/test_harpoon_provider.c
	$(CC) $(CFLAGS) -o test/test_harpoon_provider test/test_harpoon_provider.c $(LDFLAGS)

test_workspaces_provider: test/test_workspaces_provider.c
	$(CC) $(CFLAGS) -o test/test_workspaces_provider test/test_workspaces_provider.c $(LDFLAGS)

test_browser_profiles: test/test_browser_profiles.c src/profiles/browser_profiles.c $(fzf_algo_obj) $(log_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_browser_profiles test/test_browser_profiles.c src/profiles/browser_profiles.c $(fzf_algo_obj) $(log_obj) $(LDFLAGS)

test_sessions: test/test_sessions.c src/sessions/sessions.c $(fzf_algo_obj) $(log_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_sessions test/test_sessions.c src/sessions/sessions.c $(fzf_algo_obj) $(log_obj) $(LDFLAGS)

test_sessions_provider: test/test_sessions_provider.c $(fzf_algo_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_sessions_provider test/test_sessions_provider.c $(fzf_algo_obj) $(LDFLAGS)

test_profiles_provider: test/test_profiles_provider.c $(fzf_algo_obj) $(slot_store_obj) $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_profiles_provider test/test_profiles_provider.c $(fzf_algo_obj) $(slot_store_obj) $(cofi_json_io_obj) $(LDFLAGS)

test_hotkeys_provider: test/test_hotkeys_provider.c
	$(CC) $(CFLAGS) -o test/test_hotkeys_provider test/test_hotkeys_provider.c $(LDFLAGS)

test_matching_provider: test/test_matching_provider.c
	$(CC) $(CFLAGS) -o test/test_matching_provider test/test_matching_provider.c $(LDFLAGS)

test_rules_provider: test/test_rules_provider.c
	$(CC) $(CFLAGS) -o test/test_rules_provider test/test_rules_provider.c $(LDFLAGS)

# Build sinks tab parser tests
test_sinks: test/test_sinks.c
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_sinks test/test_sinks.c $(LDFLAGS)

test_sinks_provider: test/test_sinks_provider.c $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_sinks_provider test/test_sinks_provider.c $(cofi_json_io_obj) $(LDFLAGS)

test_proc_provider: test/test_proc_provider.c
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_proc_provider test/test_proc_provider.c $(LDFLAGS)

# Build projects tab parser and command tests
test_projects: test/test_projects.c $(projects_parse_obj) $(projects_commands_obj) $(projects_exec_obj) $(projects_folder_windows_obj) src/projects/projects_window_env.c src/projects/projects_window_env.h src/projects/projects_tmux_windows.c src/projects/projects_tmux_windows.h src/projects/projects_zellij_windows.c src/projects/projects_zellij_windows.h
	$(CC) $(CFLAGS) -o test/test_projects test/test_projects.c $(projects_parse_obj) $(projects_commands_obj) $(projects_exec_obj) $(projects_folder_windows_obj) $(LDFLAGS)

test_projects_provider: test/test_projects_provider.c $(cofi_json_io_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_projects_provider test/test_projects_provider.c $(config_obj) $(cofi_json_io_obj) $(LDFLAGS)

test_projects_remote_store: test/test_projects_remote_store.c src/projects/projects_remote_store.c
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_projects_remote_store test/test_projects_remote_store.c src/projects/projects_remote_store.c $(LDFLAGS)

test_projects_remote_scope: test/test_projects_remote_scope.c src/projects/projects_remote_scope.c $(projects_parse_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_projects_remote_scope test/test_projects_remote_scope.c src/projects/projects_remote_scope.c $(projects_parse_obj) $(LDFLAGS)

# Build PATH binaries tests
# (tests async-path cache dedupe/filtering, monitor hooks, and $-routing in Apps tab)
# Note: path_binaries.c compiled inline with -DCOFI_TESTING to expose test hooks
test_path_binaries: test/test_path_binaries.c src/projects/path_binaries.c $(match_obj) $(log_obj) $(tab_metadata_obj)
	$(CC) $(CFLAGS) -DCOFI_TESTING -o test/test_path_binaries test/test_path_binaries.c src/projects/path_binaries.c $(match_obj) $(log_obj) $(tab_metadata_obj) $(LDFLAGS)

# Build system actions tests
# (tests load semantics and deterministic metadata for logind-backed actions)
test_system_actions: test/test_system_actions.c $(system_actions_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_system_actions test/test_system_actions.c $(system_actions_obj) $(log_obj) $(LDFLAGS)

# Quick test targets for development
test_quick: $(match_obj)
	@if [ -f test/test_ddl.c ]; then \
		$(CC) $(CFLAGS) -o test/test_ddl test/test_ddl.c $(match_obj) $(LDFLAGS) 2>/dev/null && \
		echo "Running DDL test:" && ./test/test_ddl; \
	fi
	@if [ -f test/test_word_boundaries.c ]; then \
		$(CC) $(CFLAGS) -o test/test_word_boundaries test/test_word_boundaries.c $(match_obj) $(LDFLAGS) 2>/dev/null && \
		echo "Running word boundaries test:" && ./test/test_word_boundaries; \
	fi

# Integration tests
test_slot_store: test/test_slot_store.c $(slot_store_obj) $(cofi_json_io_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_slot_store test/test_slot_store.c $(slot_store_obj) $(cofi_json_io_obj) $(log_obj) $(LDFLAGS)

test_layout_store: test/test_layout_store.c $(layout_store_obj) $(cofi_json_io_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_layout_store test/test_layout_store.c $(layout_store_obj) $(cofi_json_io_obj) $(log_obj) $(LDFLAGS)

test_cofi_json_io: test/test_cofi_json_io.c $(cofi_json_io_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_cofi_json_io test/test_cofi_json_io.c $(cofi_json_io_obj) $(log_obj) $(LDFLAGS)

test_geometry_planner: test/test_geometry_planner.c $(geometry_planner_obj)
	$(CC) $(CFLAGS) -o test/test_geometry_planner test/test_geometry_planner.c $(geometry_planner_obj) $(LDFLAGS)

test_window_matcher: test/test_window_matcher.c $(window_matcher_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_window_matcher test/test_window_matcher.c $(window_matcher_obj) $(log_obj) $(LDFLAGS)

test_harpoon_integration: test/test_harpoon_integration.c $(harpoon_obj) $(harpoon_config_obj) $(layout_store_obj) $(matching_gc_obj) $(match_entry_obj) $(match_entry_config_obj) $(slot_store_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_harpoon_integration test/test_harpoon_integration.c $(harpoon_obj) $(harpoon_config_obj) $(layout_store_obj) $(matching_gc_obj) $(match_entry_obj) $(match_entry_config_obj) $(slot_store_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

test_matching_gc: test/test_matching_gc.c $(harpoon_obj) $(layout_store_obj) $(matching_gc_obj) $(match_entry_obj) $(match_entry_config_obj) $(slot_store_obj) $(window_geometry_matching_obj) $(geom_rule_sync_obj) $(rules_config_obj) $(geometry_planner_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_matching_gc test/test_matching_gc.c $(harpoon_obj) $(layout_store_obj) $(matching_gc_obj) $(match_entry_obj) $(match_entry_config_obj) $(slot_store_obj) $(window_geometry_matching_obj) $(geom_rule_sync_obj) $(rules_config_obj) $(geometry_planner_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

test_event_sequence: test/test_event_sequence.c $(harpoon_obj) $(match_entry_obj) $(slot_store_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj)
	$(CC) $(CFLAGS) -o test/test_event_sequence test/test_event_sequence.c $(harpoon_obj) $(match_entry_obj) $(slot_store_obj) $(window_matcher_obj) $(cofi_json_io_obj) $(log_obj) $(utils_obj) $(LDFLAGS)

test_calc: test/test_calc.c $(cofi_json_io_obj) $(tinyexpr_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_calc test/test_calc.c $(cofi_json_io_obj) $(tinyexpr_obj) $(log_obj) $(LDFLAGS)

test_calc_provider: test/test_calc_provider.c $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(cofi_tab_provider_obj) $(log_obj)
	$(CC) $(CFLAGS) -o test/test_calc_provider test/test_calc_provider.c $(calc_obj) $(cofi_json_io_obj) $(tinyexpr_obj) $(cofi_tab_provider_obj) $(log_obj) $(LDFLAGS)

test_cofi_tab_provider: test/test_cofi_tab_provider.c src/providers/cofi_tab_provider.c
	$(CC) $(CFLAGS) -o test/test_cofi_tab_provider test/test_cofi_tab_provider.c src/providers/cofi_tab_provider.c $(LDFLAGS)

test/test_plugin_boundaries.o: test/test_plugin_boundaries.c
	$(CC) $(CFLAGS) -c test/test_plugin_boundaries.c -o test/test_plugin_boundaries.o

test_plugin_boundaries: test/test_plugin_boundaries.o $(filter-out $(main_obj),$(OBJECTS))
	$(CXX) -o test/test_plugin_boundaries test/test_plugin_boundaries.o $(filter-out $(main_obj),$(OBJECTS)) $(LDFLAGS)

test_cofi_modal: test/test_cofi_modal.c src/ui/cofi_modal.c src/providers/cofi_tab_provider.c
	$(CC) $(CFLAGS) -o test/test_cofi_modal test/test_cofi_modal.c src/ui/cofi_modal.c src/providers/cofi_tab_provider.c $(LDFLAGS)

test_run_provider: test/test_run_provider.c $(tab_metadata_obj)
	$(CC) $(CFLAGS) -o test/test_run_provider test/test_run_provider.c $(tab_metadata_obj) $(LDFLAGS)

test_window_lifecycle_fixed_reset: test/test_window_lifecycle_fixed_reset.c
	$(CC) $(CFLAGS) -o test/test_window_lifecycle_fixed_reset test/test_window_lifecycle_fixed_reset.c $(LDFLAGS)

test_initial_slot_overlays: test/test_initial_slot_overlays.c
	$(CC) $(CFLAGS) -o test/test_initial_slot_overlays test/test_initial_slot_overlays.c $(LDFLAGS)


clean_tests:
	rm -f test/test_* test/*.o

-include $(shell find src -type f -name '*.d')

.PHONY: all clean install install-dev install-local install-dev-local install-service \
	check-install-path uninstall uninstall-local debug run test test-integration \
	build_tests clean_tests
