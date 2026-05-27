# Core Repeat Action

## Purpose
Core repeat action remembers the last successful Windows-tab activation query and replays it against the current live window list.

## Boundary

### Owns
- Storing the last non-empty query used for a Windows-tab activation.
- Tracking whether a repeatable Windows query exists in the current process.
- Re-filtering live windows with the stored query when repeat is requested.
- Activating, highlighting, and hiding cofi for the current top matching window.
- Treating missing state or missing live matches as a safe no-op.

### Does Not Own
- Deciding when the `.` key is eligible to trigger repeat.
- Recording provider-tab actions, command-mode actions, or arbitrary handlers.
- Persisting repeat state across cofi restarts.
- Ranking or filtering windows beyond asking the Windows filter subsystem to reapply the stored query.
- Managing selection behavior beyond resetting to the top filtered result before activation.

## Public Surface
- `core/repeat_action/repeat_action.h`
- `store_last_windows_query()`
- `handle_repeat_key()`

## Acceptance Criteria
1. `store_last_windows_query()` records a non-empty query string, truncates it
   safely to the `AppData.last_windows_query` buffer, and marks repeat state
   valid for the current process.
2. `store_last_windows_query()` ignores `NULL` and empty queries without
   clearing or overwriting any previously stored repeat state.
3. `handle_repeat_key()` returns without side effects when no valid query has
   been stored.
4. Repeat always re-runs `filter_windows()` against the current live window
   list with the stored query instead of retaining a stale window ID.
5. After re-filtering, repeat resets selection before choosing the window to
   activate, so the current top filtered match is used.
6. When the stored query no longer matches any live window, repeat does not
   activate, highlight, hide cofi, or clear the stored query.
7. When a live match exists, repeat sets workspace-switch state, activates the
   selected window, highlights it, and hides cofi.
8. Repeat state is session-only `AppData` state and is not written to disk or
   restored across process restarts.
9. The caller remains responsible for invoking `handle_repeat_key()` only for
   the Windows tab with an empty entry; this subsystem does not inspect the
   active tab or entry contents.

## Notes
The behavior intentionally repeats the last Windows query, not the exact prior window ID. If the old window closed and a different window now ranks first for the same query, repeat activates the current top match.
