# Path

## Purpose
Path provides discovery, filtering, monitoring, and launch behavior for PATH
executables from a dedicated provider-backed tab.

## Boundary

### Owns
- PATH executable discovery from the process `PATH`.
- Basename-deduped cache ownership, async scan lifecycle, and directory monitor updates.
- Ranking and filtering of cached `PathEntry` rows.
- Path-tab provider registration, row formatting, row identity, and Enter-to-launch behavior.
- The `path` command and aliases that surface the Path tab.

### Does Not Own
- Desktop application discovery, desktop-entry parsing, or system actions; those live in the Applications and System Actions subsystems.
- Provider registry, command registry, tab switching, selection mechanics, or daemon delegation.
- Terminal detection and detached terminal-process implementation beyond choosing the terminal detach helper.

## Public Surface
- `path/path_binaries.h`
- `PathEntry`
- `MAX_PATH_BINS`, `MAX_PATH_MONITORS`
- `path_binaries_filter()`, `path_binaries_ensure_loaded()`,
  `path_binaries_shutdown()`, `path_binaries_is_scanning()`
- `path/path_provider.h`
- `path_provider_register()`, `path_tab_mode()`
- Test-only hooks exported under `COFI_TESTING`:
  `path_binaries_merge_entries_test_hook()`,
  `path_binaries_on_monitor_event_test_hook()`,
  `path_binaries_reset_for_tests()`,
  `path_binaries_cap_warned_for_tests()`,
  `path_binaries_cap_warn_count_for_tests()`,
  `path_binaries_count_for_tests()`

## Acceptance Criteria
1. The Path tab is a hidden-by-default dynamic provider tab with prefix `$`,
   initial selection `0`, no shortcut hint, and command aliases `path`,
   `binaries`, `bin`, and `exe`.
2. Invoking the Path command exits command mode, records the current tab as
   prefix origin, surfaces the Path tab, and returns without launching
   anything.
3. Entering the Path tab sets the placeholder to
   `Type to filter PATH executables...`, ensures the cache is loaded, and
   filters with an empty query.
4. Query changes rebuild `filtered_path` from the cache and reset selection.
5. Empty-query results return cached entries alphabetically by basename.
6. Non-empty filtering prefilters by case-insensitive substring, ranks
   surviving matches with `match()`, sorts by descending score with basename
   tie-breaks, and copies at most `MAX_APPS` rows to the output buffer.
7. Filtering never applies the `MAX_APPS` cap during scoring; truncation
   happens only after the full scored match set is sorted.
8. The cache keeps the first winner for any duplicate basename found across
   PATH directories.
9. Cache population is asynchronous, directory-backed, and monitored via
   `GFileMonitor`; create/delete/rename events update the cache incrementally.
10. Cache overflow clamps to `MAX_PATH_BINS`, logs one warning per scan, and
    drops later entries silently.
11. Row count returns filtered rows plus one `Scanning PATH...` status row
    while the initial scan is in flight, or one `No matching PATH executables
    found` row when scanning is complete and the filtered result set is empty.
12. Real rows render one actionable cell containing the basename only; status
    rows are not actionable.
13. Row identity is `path:<exec_path>`.
14. Pressing Enter on a real row launches `exec_path` through
    `detach_launch_in_terminal_cmd()` and hides cofi; pressing Enter on a
    status row is a no-op.

## Notes
- Depends on `src/daemon/detach_launch.h` for terminal-backed detached launch.
- Depends on `src/core/app/` for `AppData` storage of `filtered_path`.
- Changes to `match()` semantics change Path-tab ranking behavior.
