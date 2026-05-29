# Geometry

## Purpose
Geometry stores, restores, and applies window layout state so users can save
where a window belongs, reapply that layout later, and keep layout restore rules
in sync with saved records.

## Boundary

### Owns
- Saved layout records keyed by `MatchEntry.match_id`, including position, size,
  desktop, maximized/fullscreen state, restore-desktop lock, and disabled state.
- Pure restore planning that decides which geometry, desktop, and window-state
  operations are necessary for a current-versus-target layout.
- Window layout capture, restore, clear, provider display, delete/toggle actions,
  and tiling option calculation.
- One-way geom-to-rules synchronization: enabled layout records cause tagged
  `geom` restore rules to exist, and disabled/deleted records remove those
  tagged rules when no enabled layout remains.

### Does Not Own
- Rule definition semantics, rule replay order, rule matching, or user-managed
  non-geom rules; those belong to the rules subsystem.
- X11/EWMH primitives such as move/resize, desktop switching, frame extents,
  size hints, workarea queries, and window state messages; geom calls x11 for
  those operations.
- Match-entry identity semantics, matching garbage collection policy, names, or
  pattern editing beyond using match ids and requesting sync after edits.
- General command parsing, overlay hosting, provider registry behavior, or UI
  rendering outside geom-specific rows and tiling overlay content.

## Public Surface
- `LayoutRecord`, `LayoutStore`, `layout_store_init()`,
  `layout_store_init_with_path()`, `layout_store_set()`,
  `layout_store_get()`, `layout_store_clear()`, `layout_store_save()`,
  `layout_store_load()`, and `layout_store_collect_ids()`
- `GeometryState`, `GeometryRestorePlan`, and `geometry_restore_plan()`
- `WindowGeometryRestoreTarget`, `resolve_window_geometry_restore_target()`,
  `apply_window_geometry_restore()`, `save_window_geometry_for_window()`,
  `restore_window_geometry_for_window()`, and
  `clear_window_geometry_for_window()`
- `geom_rule_sync_for_layout()`, `geom_rule_sync_for_pattern()`, and
  `geom_rule_sync_all_layout_patterns()`
- `geom_provider_register()`, `geom_tab_mode()`, `geom_on_query_changed()`, and
  `handle_geom_tab_keys()`
- `TileOption`, `apply_tiling()`, `create_tiling_overlay_content()`, and
  `handle_tiling_overlay_key_press()`

## Acceptance Criteria
1. Layout store initialization creates a default path at
   `$HOME/.config/cofi/layouts.json`, creating parent directories as needed, and
   test callers can override it with an explicit path.
2. `layout_store_set()` rejects missing stores, non-positive match ids, and
   non-positive dimensions; otherwise it inserts or updates exactly one record
   for the match id without duplicating records.
3. Layout records persist and reload position, size, desktop, maximized flags,
   fullscreen, restore-desktop lock, and disabled state; older files missing
   state keys load with safe defaults.
4. Clearing a layout removes that record, compacts later records, zeroes the
   vacated tail slot, and reports whether a record was actually removed.
5. `layout_store_collect_ids()` returns positive match ids in store order up to
   the caller-provided capacity so matching GC can treat saved layouts as live
   references.
6. `geometry_restore_plan()` is pure and null-safe: null inputs or already
   satisfied target state produce an all-false plan with `any == false`.
7. Restore planning unsets fullscreen/maximized states before geometry work,
   skips move/resize when the target is fullscreen or fully maximized, and sets
   requested fullscreen/maximized states after move/desktop decisions.
8. Restore planning moves the window to a valid target desktop only when the
   target desktop differs from the window desktop, and switches the active
   desktop when the target desktop differs from the viewed desktop.
9. Resolving a restore target requires an assigned match entry with a live
   `bound_x11_id` and a saved layout record; missing entries, unbound entries,
   or absent records produce no target.
10. Applying a restore target returns success without X11 changes when the
    record is disabled; invalid display, window id, width, or height fail.
11. Applying an enabled restore target reads current X11 geometry/state, asks
    `geometry_restore_plan()` for the delta, emits only the planned X11 calls,
    and flushes only when at least one operation is planned.
12. Saving geometry captures current x/y/width/height, desktop, maximized
    states, and fullscreen state from x11, reuses only a saved layout whose
    match entry matches the current title and anchors or an existing matching
    current-title entry, otherwise creates a current-title match entry, persists
    matching entries and layouts, and syncs geom rules only after the layout
    save succeeds.
13. Restoring geometry first scans enabled layout records in store order and
    applies the first whose match entry matches the current window title and
    anchors, rebinding that entry to the current window id. If no saved layout
    matches the current window, restore falls back to the selected window's
    existing binding; missing bindings or missing layouts are handled no-ops,
    and failure is reported only when an applicable layout cannot be applied.
14. Clearing geometry removes the saved layout for the selected window's match
    id, saves the layout store, runs matching garbage collection, and treats
    missing bindings or missing layout records as handled no-ops.
15. Geom rule sync creates one tagged `geom` rule with command segment `rl` per
    enabled layout record, keyed by that layout's anchored `match_id`; the rule
    stores the entry title only as pattern cache/display text.
16. Geom rule sync removes tagged geom restore rules when their layout is
    disabled or deleted, while leaving user-managed untagged `rl` rules
    untouched.
17. Startup sync scans all layout records, creates missing tagged restore rules
    for enabled layouts using their layout `match_id`, updates stale pattern
    cache text, and removes orphan tagged geom rules; stale/orphan rules are
    swept before per-layout creation so sync converges in one pass even at rule
    capacity.
18. The geom provider is hidden by default, registers the `geom`/`layouts`
    command, filters layouts by pattern, bound class, or geometry string, and
    resets selection whenever the query changes.
19. Geom provider rows show pattern, class, geometry, desktop/state flags, and
    binding status; row identity is `geom:<match_id>`. Geom rows do not look up
    Names records.
20. In the geom tab, Delete or `Ctrl+D` asks for delete confirmation; confirmed
    delete clears the layout, saves, syncs the layout's match id, runs matching GC,
    refilters, clamps selection, and refreshes display.
21. `Ctrl+L` toggles whether restore follows the saved desktop, `Ctrl+T`
    toggles layout enablement and syncs geom rules, and `Ctrl+P` opens pattern
    editing with the selected layout geometry as context.
22. Tiling fullscreen toggles fullscreen through x11's state-intent helper; all
    other tiling modes unmaximize first, choose the monitor/workarea containing
    the window, compute target geometry, apply frame and size-hint adjustments,
    move/resize frame-aware, then optionally set maximization hints through x11.
23. Tiling geometry supports half, quarter, two-thirds, three-quarters, center,
    fullscreen, and configurable two-row grid placements, including narrow,
    wide, and wider grid variants.
24. The tiling overlay renders the selected window title and current grid column
    count, maps keys to the same `TileOption` values used by command tiling, and
    hides cofi after successfully applying a valid option.

## Notes
- The geom-to-rules bridge is intentionally one-way from saved layout state to
  tagged restore rules. Rules should not reach back into geom ownership except
  by executing the public restore command.
- Disabled layout records stay in `layouts.json` and the provider list, but do
  not apply X11 changes and do not keep tagged geom restore rules alive.
- Layout restore relies on x11 frame-aware movement and state helpers. Keep
  window-manager primitives in `src/x11/`, not in this subsystem.
- Geometry restore and tiling express fullscreen/maximize changes through x11
  intent helpers. EWMH atom names stay inside `x11/`; geom owns only restore
  and tiling policy.
