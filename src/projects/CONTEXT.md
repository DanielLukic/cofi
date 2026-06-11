# Projects

## Purpose
Projects lets users jump into tmux or zellij sessions and frequently used
folders, including saved or scoped remote hosts, from one provider-backed tab.

## Boundary

### Owns
- Discovery, parsing, filtering, and display for tmux sessions, zellij sessions, zoxide folders, saved remote sessions, remote-scope rows, and locate-backed fallback folder rows.
- Projects provider registration, command aliases, shortcuts, overlays, config entries, slot payloads, session/folder launch commands, and the trigger policy for locate fallback.
- PATH executable scanning used by apps PATH mode.

### Does Not Own
- Provider registry, command registry, slot-store persistence, generic modal stack, tab rendering, or selection algorithms.
- tmux, zellij, zoxide, SSH, terminal emulator, file manager, shell behavior, or X11 activation primitives outside project-specific command/window matching.
- General apps-provider behavior beyond the PATH binary cache helper housed here.
- plocate / locate subprocess lifecycle, pattern escaping, timeout handling, or generation-guarded callback cleanup; those are delegated to `locate/`.

## Public Surface
- `projects.h`: `init_projects_mode()`, `projects_refresh()`,
  `projects_filter()`, `projects_row_count()`, `projects_format_row()`,
  `projects_match_string()`, `projects_row_identity()`, `projects_on_enter()`,
  `projects_on_query_changed()`, `projects_on_leave()`,
  `projects_on_tick()`, `projects_attach_visible()`,
  `projects_attach_named()`, `projects_has_named()`, `projects_open_folder()`,
  `projects_open_folder_terminal()`, `projects_kill_session()`,
  `projects_rename_tmux_session()`, `projects_new_session()`,
  `projects_forget_selected_remote()`, `projects_forget_remote_entry()`,
  `projects_remove_folder_entry()`, `projects_selected_session()`,
  `projects_selected_folder()`, `projects_folder_at_visible()`,
  `projects_get_shortcut_hint()`, `projects_slot_payload_for()`,
  `projects_slot_recall()`
- `projects_provider.h`: `projects_provider_register()`,
  `handle_projects_tab_keys()`
- `projects_commands.h`: `projects_build_tmux_attach_command()`,
  `projects_build_zellij_attach_command()`,
  `projects_build_tmux_kill_command()`,
  `projects_build_zellij_kill_command()`,
  `projects_build_tmux_rename_command()`, `projects_build_tmux_new_command()`,
  `projects_build_zellij_new_command()`,
  `projects_build_remote_attach_command()`,
  `projects_build_remote_new_command()`,
  `projects_build_folder_terminal_command()`,
  `projects_build_remote_folder_terminal_command()`,
  `projects_with_terminal_title()`
- `projects_exec.h`: `projects_tool_name()`,
  `projects_tool_config_value()`, `projects_resolve_tool()`
- `projects_parse.h`: `projects_parse_tmux_list()`,
  `projects_parse_zellij_list()`, `projects_parse_zoxide_list()`,
  `projects_clear_folders()`, `projects_build_folder_session_name()`,
  `projects_build_session_slot_payload()`,
  `projects_build_folder_slot_payload()`, `projects_parse_slot_payload()`,
  `projects_session_marker()`, `projects_folder_marker()`,
  `projects_format_session_match_text()`,
  `projects_format_folder_match_text()`
- `projects_folder_windows.h`: `projects_find_caja_folder_window()`
- `projects_window_env.h`: `projects_windowid_from_environ()`
- `projects_tmux_windows.h`: `projects_parse_tmux_client_pids()`,
  `projects_activate_tmux_window()`
- `projects_zellij_windows.h`: `projects_zellij_cmdline_matches_session()`,
  `projects_activate_zellij_window()`
- `projects_remote_windows.h`: `projects_remote_cmdline_matches_attach()`,
  `projects_activate_remote_attach_window()`
- `projects_remote_scope.h`: `projects_remote_scope_init()`,
  `projects_remote_scope_begin_fetch()`, `projects_remote_scope_apply()`,
  `projects_remote_scope_is_active()`, `projects_remote_scope_is_loading()`,
  `projects_remote_scope_current_host()`, `projects_remote_scope_clear()`,
  `projects_remote_scope_status_message()`,
  `projects_remote_scope_clear_status_message()`
- `projects_remote_store.h`: `projects_remote_store_init()`,
  `projects_remote_store_reload()`, `projects_remote_store_count()`,
  `projects_remote_store_entry_at()`, `projects_remote_store_append_sessions()`,
  `projects_remote_store_forget()`, `projects_remote_store_save_intent()`
- `overlay_projects.h`: `create_project_kill_overlay_content()`,
  `create_project_rename_overlay_content()`,
  `create_project_new_overlay_content()`,
  `create_project_remote_host_overlay_content()`,
  `handle_project_kill_key_press()`, `handle_project_rename_key_press()`,
  `handle_project_new_key_press()`, `handle_project_remote_host_key_press()`
- `locate/projects_locate.h`: `ProjectsLocateResult`,
  `ProjectsLocateResultsCallback`, `projects_locate_set_results_callback()`,
  `projects_locate_search_async()`, `projects_locate_cancel_pending()`
- `path_binaries.h` (apps PATH-mode cache helper):
  `path_binaries_ensure_loaded()`, `path_binaries_filter()`,
  `path_binaries_is_scanning()`, `path_binaries_shutdown()`
- Test-only hooks exported under `COFI_TESTING`:
  `projects_set_launch_impl_test_hook()`,
  `projects_set_command_impl_test_hook()`,
  `projects_set_argv_launch_impl_test_hook()`,
  `projects_set_exec_impl_test_hook()`,
  `projects_remote_scope_reset_for_test()`,
  `projects_remote_scope_set_exec_for_test()`,
  `projects_remote_scope_load_from_outputs_for_test()`,
  `projects_remote_scope_set_loading_for_test()`,
  `projects_remote_scope_set_active_for_test()`,
  `projects_remote_scope_fetch_sync_for_test()`,
  `projects_remote_scope_set_status_for_test()`,
  `projects_remote_scope_build_ssh_argv_for_test()`,
  `projects_remote_store_set_path_for_test()`,
  `projects_remote_store_reset_for_test()`,
  `projects_remote_store_add_for_test()`,
  `projects_remote_store_save_for_test()`,
  `path_binaries_merge_entries_test_hook()`,
  `path_binaries_on_monitor_event_test_hook()`,
  `path_binaries_reset_for_tests()`, `path_binaries_cap_warned_for_tests()`,
  `path_binaries_cap_warn_count_for_tests()`,
  `path_binaries_count_for_tests()`

## Acceptance Criteria
1. Registering projects creates an optional hidden dynamic `PROJECTS` tab with hide-on-esc modal policy, initial selection `0`, 1500 ms refresh tick, row/query/Enter/key hooks, and slot storage enabled.
2. Registration also registers `projects.tmux_path`, `projects.zellij_path`, `projects.zoxide_path`, `projects.file_explorer_path`, `projects.locate_enabled`, `projects.locate_excludes`, `projects.locate_timeout_ms`, and `projects.locate_search_roots`; tool-path keys accept empty-for-PATH or an absolute executable path and reject relative or non-executable paths.
3. The command surface registers `projects` with aliases `project`, `tmux`, `tx`, `zj`, and `zellij`, help `projects, project, tmux, tx, zj, zellij [@SLOT|SESSION]`, and hotkey auto-open behavior.
4. Running the command without args exits command mode, records the origin tab, surfaces projects, and leaves the window open; session-name args refresh, attach, and hide on success.
5. `@SLOT` command args resolve payloads from the `projects` slot namespace; invalid args show `No matching tmux/zellij session.` without hiding.
6. Entering the tab sets placeholder `projects...`, registers the locate callback sink, and refreshes local rows or active remote-scope rows.
7. Local refresh clears old folders, loads tmux, zellij, saved remote intents, and zoxide rows, and preserves tmux errors in `last_error` when no tmux sessions are available.
8. Active remote scope replaces local rows with scoped remote sessions/folders; loading scope exposes `Loading remote projects for <host>...`.
9. Empty query lists sessions first and folders second in discovery order; non-empty query fuzzy-matches formatted row text and sorts by score, then original order.
10. With no filtered rows, the provider returns one status row: `No matching projects`, current error as `COFI_ROW_ERROR`, or `No tmux/zellij sessions or zoxide folders`.
11. tmux rows render `[t]`, name, window count, and client count; zellij rows render `[z]`, name, and blank count columns; zoxide folder rows render `[d]`, label, and path; locate rows render `[~]`, label, and path.
12. Remote folder/session display prefixes labels with `[REMOTE:<host>]`; all real project rows are actionable and slottable.
13. Match text includes row marker and searchable details; row identity is folder path for folders and session name for sessions.
14. Query changes clear remote status, cancel pending locate work, refilter, and reset selection; periodic ticks refresh while preserving selected row type, backend, and identity when the query is empty.
15. When a non-empty local query has at least three characters and locate is enabled, Projects starts the locate fallback asynchronously in parallel with the primary tmux/zellij/zoxide filter results; if `projects.locate_search_roots` is configured, only locate candidates under those roots are eligible.
16. Locate callback results append temporary `[~]` folder rows, preserve row selection by identity across the refilter, and disappear on the next query/filter rebuild unless re-supplied by the current locate generation.
17. In merged query ranking, tmux sessions, zellij sessions, and zoxide folders get a fixed score bonus over locate rows so navigation-oriented primary results stay ahead unless locate has a substantially stronger match.
18. Pressing Enter on a folder activates a matching Caja window when possible, otherwise uses configured file explorer, then `caja`, `xdg-open`, then `gio open`; remote folders open as `sftp://<host><path>`.
19. Pressing Enter on a session activates an existing tmux, zellij, or remote attach window when detectable before launching a terminal attach command.
20. New, kill/delete, and rename overlays build the corresponding tmux/zellij/zoxide commands; rename is tmux-only, and successful mutating actions refresh projects.
21. Ctrl+S fetches remote rows over SSH with X-forwarding, BatchMode, and short timeout; remote attach/new commands use `ssh -X -t`, save successful session intents to `projects.json`, and reload saved intents as remote rows.
22. Slot payloads are typed as `session:tmux:<name>`, `session:zellij:<name>`, or `folder:<path>`; parsing preserves colons inside names/paths and rejects empty or unknown payloads.
23. Shortcut hints are row-sensitive: folder rows include `Ctrl+T=Terminal`, tmux rows include `Ctrl+R=Rename`, and delete remains available only for mutable rows even though the shortcut text stays shared.
24. PATH binary scanning caches executable basenames from PATH, keeps the first duplicate-name winner, sorts empty-query results by name, scores substring matches, and returns at most `MAX_APPS` entries.
25. PATH monitoring adds, removes, and renames cached executables live; cache cap overflow emits one warning and clamps the cache to `MAX_PATH_BINS`.
26. WINDOWID environment parsing scans NUL-separated `/proc/<pid>/environ`
    data, accepts only a non-zero decimal `WINDOWID=<id>` with no trailing
    junk, writes `0` on failure, and never reads beyond the supplied byte
    length.

## Notes
- Remote project state has two layers: active remote scope is transient, while saved remote session intents persist in `projects.json`.
- Project command strings intentionally use shell quoting because tmux, zellij, SSH, and terminal-title wrapping are launched through a shell command string.
- `path_binaries.*` lives here because it supports command/project workflows, but callers should treat it as the apps PATH-mode cache helper.
