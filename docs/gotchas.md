# Cofi Gotchas And Invariants

Intent: this file captures implementation learnings, fragile behaviors, and regressions to avoid while changing the codebase. It is not a feature spec and not a session log.

See also:
- `CLAUDE.md` for contributor workflow, build rules, and subsystem map
- `checkpoint.md` for current branch/session state and recent work
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

## Docs And Truth Sources

- Treat `SPEC.md` as the behavior spec.
  It is the best source for intended user-visible behavior.

- Treat `checkpoint.md` as current-state operational context.
  It is useful for recent fixes, outstanding debt, and what changed lately, but it is not the canonical feature spec.

- Treat `COFI_PRD.md` as partially stale unless refreshed.
  It still mentions old architecture such as D-Bus-based single-instance behavior.
  Verify against code and recent commits before relying on it.

## Fixed Window Sizing (TFD-100)

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
  When docs disagree, trust the implementation, `SPEC.md`, `checkpoint.md`, and the newest relevant commits first.
