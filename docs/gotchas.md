# Cofi Gotchas And Invariants

Intent: this file captures implementation learnings, fragile behaviors, and regressions to avoid while changing the codebase. It is not a feature spec and not a session log.

See also:
- `CLAUDE.md` for contributor workflow, build rules, and subsystem map
- `SPEC.md` for intended product behavior

## Hotkeys

- Hotkey parsing currently exists in two places.
  `src/utils.c` contains the richer user-facing shortcut parser with aliases, case-insensitive matching, and typo suggestions.
  `src/hotkeys.c` still contains the X11 grab parser used during actual `XGrabKey` registration.
  If shortcut syntax changes, keep both paths aligned or consolidate them deliberately.

- Visible-window hotkey handling is a special path.
  Pressing a global hotkey while cofi is already visible must not behave like a fresh open-from-hidden flow.
  The visible path cancels focus-loss timers and may switch tabs or step selection inline.

- X11 grabs can cause synthetic focus events.
  A global hotkey can trigger `FocusOut(NotifyGrab)` on the cofi window.
  If the focus-loss close timer is left running, cofi may close incorrectly while the user is interacting with it.
  Preserve the timer-cancel behavior in the visible-window hotkey path.

- Repeated `Alt+Tab` while visible is intentional.
  In Windows mode, repeated `Alt+Tab` advances deeper through MRU history.
  Do not change this to a noop or reopen behavior unless that is a deliberate product decision.

- Hotkey config changes must re-grab live.
  Editing, adding, or removing a hotkey is not complete until grabs are refreshed with `regrab_hotkeys()`.
  Saving config without re-registering leaves runtime behavior stale until restart.

- Overlay-driven hotkey changes must refresh list state explicitly.
  Do not rely on re-setting identical `GtkEntry` text to trigger `changed` and rebuild the Hotkeys tab.
  GTK may treat that as a no-op, leaving the filtered list stale until the user changes tabs or filter text.

- After overlay add/edit, refresh in the full UI-state order.
  Use `filter_hotkeys()` first, then `validate_selection()`, then `update_scroll_position()`, then `update_display()`.
  If scroll position is not updated after moving selection, the selected binding may be off-screen in long lists.

- Overlay code may need direct access to filter helpers.
  If a modal add/edit flow changes tab data, the relevant `filter_*()` function cannot be treated purely as a local implementation detail in the main event loop.
  Keep that boundary practical enough for overlays to rebuild visible state immediately.

- Startup hotkey conflicts are a first-class flow.
  Grab failure is not just an exceptional log line; the Retry/Exit dialog is part of normal startup/error handling.
  Changes to hotkey setup should preserve conflict reporting quality.

- The `!` suffix changes dispatch mode.
  Hotkey commands ending in `!` auto-execute immediately.
  Commands without `!` prefill command mode for editing.
  Changes here must be checked both when cofi is hidden and when it is already visible.

## Command And Helper Boundaries

- Keep side effects out of low-level movement helpers.
  The `:tm` regression came from `move_window_to_next_monitor()` containing UI lifecycle behavior (`gtk_main_quit()`) that belonged in the command layer.
  Helpers should move windows; command handlers should decide activation, hiding, quitting, and follow-up UI behavior.

- Avoid duplicate activation paths.
  `cmd_toggle_monitor` already activates the commanded window after moving it.
  Reintroducing activation inside the lower-level move helper can cause double-activation or control-flow bugs.

## Provider Tabs And Plugin Architecture

- Provider key hooks must be verified through the focused entry path.
  Unit tests that call `CofiTabProvider.handle_key` or `on_enter_pressed` directly prove the hook logic, but not GTK delivery.
  Keys such as `Return` and `Ctrl+E` can be consumed by `GtkEntry` defaults unless the live `key-press-event` wiring is covered by an Xvfb test.
  Handlers must not mutate state on the path to `return FALSE`, because the same event can be seen by both the focused entry and the window.

- Provider rendering is inverted like the rest of cofi's list UI.
  `format_provider_display` renders from `end - 1` down to `scroll`.
  Do not "fix" provider display to render raw index 0 at the top unless the entire display pipeline changes with it.

- Keep raw provider rows separate from filtered/visible rows.
  Provider row callbacks (`format_row`, `match_string`, `row_identity`, `slot_payload_for`) take raw provider indices.
  Core selection uses filtered indices and maps them through `cofi_filtered_to_raw()`.
  Mixing these works on unfiltered lists and fails once query filtering or scoring changes row order.

- `on_enter_pressed` intentionally receives both filtered and raw indices.
  The filtered index is useful for UI state; the raw index is the provider's stable row lookup.
  Ported providers should not recover raw indices manually when core already supplies them.

- Provider tabs should use provider-owned hooks for tab behavior.
  Tab-specific key handling belongs on `CofiTabProvider.handle_key`; list lifecycle belongs on `on_enter`, `on_leave`, and `on_query_changed`.
  Reintroducing provider cases in `key_handler.c`, `selection.c`, or `display.c` is usually architecture drift.

- Provider key hooks get first refusal before generic provider slots.
  A slottable provider may still need tab-local shortcuts such as Projects `Ctrl+N`.
  `handle_key` should return `TRUE` only for keys it owns; returning `FALSE` lets `Ctrl+key` / `Alt+key` continue into generic slot assignment and recall.

- Dynamic provider tabs are not `TAB_COUNT`.
  `TAB_COUNT` is the sentinel for the core tab enum. Runtime provider handles start after it.
  Use registry helpers such as `cofi_list_provider_tabs()` instead of looping from `TAB_WINDOWS` to `TAB_COUNT` when you mean "all visible provider tabs."

- Provider enablement is stronger than hiding a tab.
  A disabled provider must fail closed across tab lookup, command lookup, prefix lookup, command candidates/help, and slots.
  Do not use `tab_visibility` as the disable gate; `show_all_tabs` intentionally overrides hidden visibility but must not resurrect disabled providers.

- Header changes require stale-daemon awareness.
  Large `AppData` layout changes shift offsets used by the running daemon.
  After changing `AppData`, provider state structs, or tab visibility arrays, rebuild and restart before judging behavior; otherwise the old daemon can read garbage and produce unrelated-looking failures.

- Preserve selection before model-state mutation.
  Providers must call `preserve_selection()` BEFORE wiping or replacing the
  data structures that back `row_identity()`; otherwise restore falls back even
  when the same logical row still exists. `src/bluetooth/bluetooth_model.c`
  `apply_snapshot()` is the working example. Selection drift or jumps to row 0
  are the symptom of getting this wrong.

- Stable model sort must happen before refilter.
  Providers that refresh external data and preserve selection by identity need
  a deterministic model order before any filtered-index rebuild. Bluetooth uses
  alias-ascending with path tiebreak; without that sort, refresh ticks can
  reshuffle rows under a stable identity-preserved selection.

- Refilter after row delete before selection validation.
  If a provider deletes a raw row, it must rebuild its filtered list before `validate_selection()`. Otherwise the old filtered index can point at a now-missing raw slot and the UI shows `(missing)` until another refresh.
  Reference: `fe816a7`.

- No sync D-Bus calls in `src/bluetooth/`.
  All Bluetooth D-Bus work must go through `g_dbus_connection_call()` async
  with explicit `timeout_msec`. `_sync` variants are forbidden in this
  subsystem because cofi must never block on Bluetooth operations.

- `src/system_actions/system_actions.c` is the anti-pattern for new D-Bus work.
  It uses `g_dbus_proxy_call_sync` and related blocking helpers throughout.
  Do not copy from it. Scaffold async D-Bus work from `src/bluetooth/` or the
  async result patterns in `src/projects/` instead.

## Command Targeting Ordering (TFD-511)

- `command_target_id` must be captured before `show_window()` when entering command mode from hidden/delegated flows.
  Required ordering is `get_active_window_id()` → `show_window()` → `enter_command_mode()`.

- Daemon-socket dispatch preserves this ordering for `--command` / command opcode paths.
  Hidden hotkey command-mode dispatch follows the same sequence.

- Regression risk: if `show_window()` is moved before capture, command targeting can drift to the wrong window.
  TFD-511 behavior depends on identity pinning from the pre-show active window, not post-open selection side effects.

## Docs And Truth Sources

- Treat `SPEC.md` as the behavior spec.
  It is the best source for intended user-visible behavior.

- Treat `COFI_PRD.md` as partially stale unless refreshed.
  It still mentions old architecture such as D-Bus-based single-instance behavior.
  Verify against code and recent commits before relying on it.

## Window List And Identity

- Title preservation across window-list refresh matters.
  `get_window_list()` must not overwrite cached titles for existing windows. For known windows, `handle_window_title_change()` is the sole authority for title updates; client-list scans should read titles only for newly discovered window IDs.
  Reference: `4ec2ce9`.

- Geom restore matches by current title, not bound X11 id.
  Restore that treats `bound_x11_id` as the primary lookup breaks when a restarted window comes back with a new X11 id. The stable path matches by current title plus class/instance/type anchors, with the bound X11 id used only as a disambiguation hint among otherwise identical candidates.
  Reference: `b162a22`.

## Fixed Window Sizing (TFD-100)

- Fixed width is enforced at the final text-buffer boundary.
  Provider column hints are still required for well-formed rows, but they are not the only guard.
  Any content passed to `gtk_text_buffer_set_text()` must already be clipped to `get_display_columns(app)` line by line; otherwise GTK can expand the toplevel horizontally for a long unwrapped line.

- `fixed_window_size_initializing` flag is cleared via `g_idle_add` after `gtk_window_resize()` in `init_fixed_window_size()`.
  `gtk_window_resize()` is async — the resize-triggered `size-allocate` on the window (which fires `on_window_size_allocate`) may arrive after the idle clears the flag, letting the reposition callback run during init.
  In practice the reposition is idempotent so this is low impact, but if you touch the fixed sizing init path, be aware of this ordering hazard.
  A cleaner fix would compare the current allocation against the target size inside `on_window_size_allocate` rather than relying on the flag.

## Workspace Slots And Occlusion

- Digit slots are intentionally stricter than "any visible pixel."
  The design target is "reachable by eye": only meaningfully visible windows should receive Alt+1-9 workspace slots.

- Occlusion uses rectangle subtraction, not summed overlap.
  Do not go back to accumulating occluder overlap percentages; overlapping occluders double-count and can report impossible values like >100% occluded.

- Use outer/frame geometry for subtraction, but content rects for visibility.
  Shrinking windows to content rects before subtraction caused regressions. The correct rule is: subtract using full window geometry, then clip surviving fragments to the target window's content rect before counting visible area.

- Decoration leaks must not count as visible content.
  Frame extents are used so titlebars/borders do not keep a heavily covered window eligible for a digit slot.

- Total visible scraps are not enough by themselves.
  A window must clear the configured threshold both for total visible content fraction and for its largest single visible content fragment. This prevents fragmented leftovers from qualifying.

- Thin slivers should not receive digit slots.
  The largest visible fragment must also satisfy the minimum visible dimension gate (currently 8x8 px).

- Slot overlays should track the meaningful visible area.
  Place the overlay at the center of the largest visible fragment, not the raw window center and not a weighted average across multiple fragments.

- `slot_occlusion_threshold` is an integer percent.
  `5` means 5 percent. Keep legacy float config compatibility in the loader, but write/save the modern integer-percent form.

## Geometry And Frames

- `unmaximize_and_settle()` is mandatory before geometry changes on maximized windows.
  Clearing maximize atoms is asynchronous at the WM boundary. If code sends `_NET_WM_STATE_REMOVE` and immediately calls `xmove_resize_frame_aware()` or `XMoveResizeWindow()`, the WM can still treat the window as maximized and override or ignore the new geometry. Always use `unmaximize_and_settle()`, not bare `set_window_maximized(..., UNSET)`.
  Reference: `45e009f`.

- CSD `_GTK_FRAME_EXTENTS` handling is required for tiling and geometry work.
  CSD windows often have no `_NET_FRAME_EXTENTS`, while raw `XGetGeometry` still includes invisible shadow margins. Tiling and geometry code must check `_GTK_FRAME_EXTENTS` when `_NET_FRAME_EXTENTS` is absent or all-zero; otherwise the visible content frame is placed incorrectly in the work area.
  References: `26c4875` for tiling, `da4283e` for slot assignment.

## Repeat Last Action

- Repeat-last-action is intentionally narrow in v1.
  It applies only on the Windows tab, only for the current session, and only when `.` is pressed with an empty query.

- Repeat stores queries, not window identities.
  The saved state is the last successful non-empty windows-tab query-driven activation. Replay must re-filter the live list and activate the current top match, not a stale window ID.

- Empty-query Enter must not overwrite repeat state.
  Storing `""` turns repeat into a generic "activate current top window" shortcut, which is explicitly out of scope for v1.

- `.` must insert normally when the query is non-empty.
  The repeat intercept only applies to the Windows tab with an empty entry.

## Apps Tab Filtering

- Do not concatenate `name`, `generic_name`, and `keywords` into one search string.
  `has_match` is a subsequence check; concatenation creates cross-field false positives.
  Example: `audac` can match Atril via characters spread across multiple fields.
  Match each field independently instead.

- Do not treat the keywords field as one big string either.
  Space-joined keywords let subsequence matching hop across unrelated tokens.
  Example: `thu` can match Audacity via letters spread across separate keyword words.
  Split keywords into tokens and match each token independently.

- Apps ranking is local to the Apps launcher.
  Do not "fix" Apps behavior by changing the shared Windows-tab fuzzy/MRU ranking pipeline.

- These invariants are regression-tested in `test/test_apps.c`.

## Testing

- Do not assume all test entrypoints cover the same set.
  `make test` and `test/run_tests.sh` are not guaranteed to stay in sync.
  Check both before assuming a test is part of the default suite.

- Header changes still require a clean rebuild discipline.
  The Makefile now has generated header deps, but the repo workflow still expects `make clean && make` after header changes.

## Working Rules

- Be careful with hidden-vs-visible state.
  A large share of regressions come from code that works when cofi is hidden but behaves differently when the window is already open.

- Prefer current code plus recent commits over older prose.
  When docs disagree, trust the implementation, `SPEC.md`, and the newest relevant commits first.

- `check_rule_match()` must get `allow_fire=false` for non-firing scans.
  `allow_fire=true` mutates per-window transition state. Callers that are only seeding state, such as startup client-list dispatch over already-existing windows, must pass `allow_fire=false` or `once=true` rules can be consumed before they ever legitimately fire.
  Reference: `e2f9856`.

## PATH Binary Launcher

- **Basename dedupe: first in $PATH wins.** When the same binary name appears in multiple PATH directories, only the first (highest-priority) entry is kept. No `.desktop`-style shadow or merge is applied.

- **Cache cap is MAX_PATH_BINS (4096).** A single overflow warning is emitted per scan; later PATH entries are silently dropped. Tested by `test_global_cap_overflow_sets_warned`.

- **Filter output cap is MAX_APPS (512), applied at copy-out — NOT during scoring.** Score ALL substring-matching entries first (into `scored[MAX_PATH_BINS]`), sort by score descending, then truncate to MAX_APPS. Applying the cap during the scoring loop drops high-score entries that appear late in the alphabetically-sorted cache when more than 512 entries match. (Was the audit-batch-b bug; regression-tested by `test_large_match_set_prefers_high_score`.)

- **GFileMonitor cap is MAX_PATH_MONITORS (64) PATH directories.** Defined in `src/path_binaries.h`. If this needs to become configurable, change the constant there.

- **`$` routing lives in `src/tab_switching.c:filter_apps`.** The check `if (query[0] == '$')` redirects to `path_binaries_filter`. Do not add a second copy of this check elsewhere; PATH binaries would silently receive both the raw query and the stripped query.

## Process Detachment

- **`detach_launch_properly` tries systemd-run first.** It probes for `systemd-run`, builds `["systemd-run", "--user", "--scope", "--", ...]` and spawns. If unavailable or spawn fails, it falls back to fork + setsid + double-fork + execvp.

- **The errno-pipe is the correct exec-failure propagation mechanism.** The double-fork makes the grandchild's exit status invisible to the original parent. The errno-pipe (`pipe()` + `FD_CLOEXEC` on the write end) solves this: a successful `execvp` closes the write end automatically; failure writes errno bytes that the parent reads. An empty read is success.

- **Terminal launches are not generic detached app launches.** Commands that run inside a terminal must use a user-scope `systemd-run` when available, but the terminal emulator process must not have stdin/stdout/stderr redirected to `/dev/null`. The user scope keeps terminal clients outside `cofi.service`'s cgroup so they survive cofi restarts; normal stdio keeps interactive tools such as Claude, Codex, tmux, and zellij able to create and use their PTY. The fork+setsid fallback is best-effort only and may still die with service cgroup cleanup.

- **Terminal launches always keep the explicit `sh -c` wrapper.** Argv-split terminals use `{term, -e, sh, -c, cmd}`. Mate/GNOME terminals use the modern `{term, --, sh, -c, cmd}` form instead of deprecated `-e`. Without the shell wrapper, multi-word commands are mishandled on argv-split terminals.

- **Projects tool resolution must be shared across list and action paths.** `tmux`, `zellij`, `zoxide`, and the configured file explorer are resolved from Projects config first, then the cofi service `PATH`. Do not hardcode user/mise paths or update only `projects_refresh.c`; attach/new/kill/rename and existing-window activation must use the same resolved executable.

- **GUI desktop entries must not go through a shell.** `Exec=` is argv-like, not shell syntax. The correct path is `g_shell_parse_argv` (handles quoting/escaping only) followed by `detach_launch_argv_array` (direct execvp). Shell metacharacters in `Exec=` must not execute.

- **Do not use `GSubprocessLauncher` for app launches.** It inherits cofi's cgroup. Launched apps then die when cofi's cgroup is cleaned up (e.g. `systemctl --user stop cofi`). This is the root cause of TFD-557.

## Terminal Detection

- **Detection priority chain:** `$TERMINAL` (if resolvable in PATH) → desktop-env configured terminal → `x-terminal-emulator` → hardcoded candidates → `xterm` fallback.

- **`x-terminal-emulator` is NOT always the right answer.** It is the Debian alternatives-system pointer and may point to an unexpected terminal. On MATE/GNOME/Cinnamon sessions the correct approach is `gsettings get org.gnome.desktop.default-applications.terminal exec`. On XFCE use `xfconf-query`. On KDE fall back to `konsole` directly.

- **The desktop-env step is injectable for tests.** The `DesktopTerminalGetter` typedef allows test stubs to simulate specific desktop sessions without needing a live `gsettings` or `xfconf-query`. Tests use `detect_terminal_with_desktop_for_test(resolver, getter)`. Keep the getter parameter in any refactor of `detect_terminal_with_resolver`.
