# Core History

## Purpose
Core history maintains the in-memory window ordering used by the window switcher so cofi can present recent, live windows in a stable Alt-Tab-friendly order.

## Boundary

### Owns
- Synchronizing `AppData.history` with the current live `AppData.windows` list.
- Promoting the newly active non-cofi window to the front of window history.
- Reordering non-leading history entries by workspace and window type when cofi uses its own ordering.
- Preserving the first two history slots so repeated Alt-Tab can switch between the active and previous windows.

### Does Not Own
- Discovering windows from X11 or populating `AppData.windows`.
- Native X11 stacking-order mode, filtering, scoring, or display rendering.
- Command-mode, run-mode, calculator, emoji, or other feature-specific histories.
- Persisting window history across cofi restarts.

## Public Surface
- `core/history/history.h`
- `update_history()`
- `partition_and_reorder()`

## Acceptance Criteria
1. `update_history()` removes windows from `AppData.history` when their IDs no
   longer appear in the current `AppData.windows` list.
2. Existing history entries are refreshed with the latest `WindowInfo` metadata
   from the current window list without changing their order unless activation
   changes require it.
3. Newly discovered windows are appended to history in current window-list order
   until `MAX_WINDOWS` is reached.
4. When the active X11 window changes to a tracked non-cofi window, that window
   moves to `history[0]` and the previous entries shift back in MRU order.
5. A window whose class name is `cofi` is never promoted to the front of
   history when it becomes active.
6. If the active window is unchanged or no active window is reported, history
   synchronization does not perform an MRU promotion.
7. `partition_and_reorder()` leaves histories of two or fewer windows unchanged.
8. For longer histories, `partition_and_reorder()` preserves the first two
   entries and reorders the remaining windows as: current-desktop Normal,
   other-desktop Normal, current-desktop non-Normal, other-desktop non-Normal,
   then sticky windows.
9. History count never grows beyond `MAX_WINDOWS`, and entries beyond that cap
   are not written.

## Notes
This history is session-local window ordering only. Other subsystems use the word "history" for independent command, run, calculator, emoji, or provider-specific recall features; those are outside this folder.
