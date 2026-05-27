# Core Selection

## Purpose
Core selection owns the active row index, selected identity, and scroll offset for cofi's window list and provider-backed list surfaces.

## Boundary

### Owns
- Initializing and resetting `AppData.selection` for windows and provider tabs.
- Moving the selected row through visible windows or provider rows with wraparound.
- Preserving and restoring selection by window ID or provider row identity across filtering.
- Clamping selected indices and scroll offsets so the selected row stays visible.
- Notifying provider tabs when user navigation changes their selected row.

### Does Not Own
- Filtering, scoring, sorting, or building the row lists being selected.
- Executing actions for the selected row.
- Rendering row contents or deciding the display height.
- Alt-Tab special-case selection policy outside the generic move and restore operations.
- Persisting selection across cofi restarts.

## Public Surface
- `core/selection/selection.h`
- `init_selection()`, `reset_selection()`
- `get_selected_window()`, `get_selected_index()`
- `move_selection_up()`, `move_selection_down()`
- `preserve_selection()`, `restore_selection()`
- `validate_selection()`
- `update_scroll_position()`, `get_scroll_offset()`, `set_scroll_offset()`

## Acceptance Criteria
1. `init_selection()` starts all row indices, remembered IDs, and scroll
   offsets at their neutral values without requiring a populated window or
   provider list.
2. `reset_selection()` selects the first filtered window on the Windows tab,
   or the provider's `initial_selection_index` clamped into the provider row
   range on provider tabs.
3. `get_selected_window()` returns `NULL` outside the Windows tab, for empty
   filtered results, or when the window selection index is out of range.
4. On the Windows tab, `move_selection_up()` advances to the next filtered
   row index and wraps from the last row to index `0`; `move_selection_down()`
   decrements the row index and wraps from index `0` to the last row.
5. On provider tabs, selection movement uses the provider `row_count()` range,
   wraps at both ends, refreshes the display, and calls
   `on_selection_changed()` when the provider supplies it.
6. `preserve_selection()` records the selected window ID on the Windows tab
   and records the provider `row_identity()` on provider tabs that expose one.
7. `restore_selection()` returns to the preserved window ID or provider row
   identity when that row still exists after filtering.
8. When the preserved row no longer exists, `restore_selection()` falls back
   to the first filtered window or the provider's clamped initial row.
9. `validate_selection()` clamps Windows-tab selection into
   `[0, filtered_count)` and clears the selected window ID when no filtered
   windows exist.
10. `validate_selection()` clamps provider selection down to the last row
   when it is past the provider row count, and resets to `0` for empty
   provider results.
11. `update_scroll_position()` keeps the selected row within the visible
   display range and resets the active scroll offset to `0` when all rows fit.
12. `get_scroll_offset()` and `set_scroll_offset()` read and write the offset
   for the active tab family without changing offsets for inactive tab families.

## Notes
Provider-row selection depends on stable `row_identity()` values. Providers that cannot expose stable identities should expect selection to fall back to their initial row after filtering.
