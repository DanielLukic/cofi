# `:geom` / `:layouts` tab — spec

## Goal

Surface `layout_store` records as a tab so the user can see, manage, and delete saved layouts. Three actions: **delete**, **workspace-lock toggle**, **enable/disable toggle**.

## What ships

- New `src/geom_provider.[ch]` — `CofiTabProvider` mirror of `harpoon_provider.c`. Filter logic inline (no separate `filter_geom.c`).
- New `test/test_geom_provider.c`.
- New `LayoutRecord` fields: `bool restore_desktop` (default true), `bool disabled` (default false).
- Rename existing command `:clear-layout` → `:delete-layout` (alias `:dl`) for naming consistency with the tab action.
- Wire `app->filtered_geom[MAX_WINDOWS] + filtered_geom_count` in `app_data.h`.
- `geom_provider_register()` in `builtin_plugins.c`.

## Display

- **Tab name:** `LAYOUTS` (matches data noun; consistent with `HARPOON`/`MATCHING`/`HOTKEYS`).
- **Commands:** `:geom` (primary) + `:layouts` (alias).
- **No `:g` alias** — too valuable to burn on the 16th provider's shortcut.
- **Placeholder text:** `"search saved layouts"` (matches harpoon/matching convention).
- **Empty state:** no custom row. Zero records render as zero rows (other tabs' convention).

## Row schema

| # | Cell | Width | Source |
|---|---|---|---|
| 0 | label | 25 | `MatchEntry.original_title` pattern only |
| 1 | class | 18 | `MatchEntry.class_name` |
| 2 | geometry | 18 | `"WxH+X+Y"` from `LayoutRecord` |
| 3 | desktop + state | 14 | `"d=N [V][H][F][L][D]"` — V/H/F = max-vert/horz/full; **L** = workspace-locked (restore_desktop=true); **D** = disabled |
| 4 | bound | 8 | `"bound"` if `MatchEntry.assigned`, else `"unbound"` |

`row_flags = COFI_ROW_ACTIONABLE`.

**Missing-MatchEntry case:** by GC invariant, every `LayoutRecord` has a corresponding `MatchEntry`. If lookup ever fails, `log_warn` and skip the row. Do not invent a UI label for an impossible-at-runtime state. (On-disk corruption recovery is a separate TBD discussion, not handled in v1 UI.)

## Provider registration

Mirror `harpoon_provider_register`:

```c
s_geom_provider.tab_mode         = COFI_PROVIDER_DYNAMIC_TAB;
s_geom_provider.id               = "geom";
s_geom_provider.display_name     = "LAYOUTS";
s_geom_provider.required         = 0;
s_geom_provider.hidden_by_default = 1;
s_geom_provider.modal_policy     = COFI_MODAL_HIDE_ON_ESC;
s_geom_provider.row_count        = geom_row_count;
s_geom_provider.format_row       = geom_format_row;
s_geom_provider.match_string     = geom_match_string;
s_geom_provider.row_identity     = geom_row_identity;  /* "geom:<match_id>" */
s_geom_provider.on_enter         = geom_on_enter;
s_geom_provider.on_query_changed = geom_on_query_changed;
s_geom_provider.handle_key       = handle_geom_tab_keys;
s_geom_provider.shortcut_hint    = "Ctrl+D=Delete  Ctrl+L=Lock workspace  Ctrl+T=Toggle enable";
```

**Don't reflexively copy** `prefix_origin_tab` pattern from harpoon — verify geom needs return-to-prior-tab on Esc before cloning.

## Filter

Inline in `geom_provider.c` (~30 LOC). Iterate `app->layouts.records[0..count]`, build searchable string from `(label, class, instance, geometry)`, `has_match`, populate `app->filtered_geom`/`filtered_geom_count`.

## Actions

### Delete (`Ctrl+D` / `Delete`)

Confirm via `show_confirm_overlay` (shared primitive, ADR-0010):

```c
static int s_pending_delete_match_id = 0;

static void do_delete(AppData *app) {
    int match_id = s_pending_delete_match_id;
    s_pending_delete_match_id = 0;
    if (layout_store_clear(&app->layouts, match_id)) {
        layout_store_save(&app->layouts);
        matching_run_gc(app);
        geom_on_query_changed(app, gtk_entry_get_text(GTK_ENTRY(app->entry)));
        reset_selection(app);
        update_display(app);
    }
}

static void show_geom_delete_confirm(AppData *app, int match_id) {
    s_pending_delete_match_id = match_id;
    /* build info markup (escape user-controlled values) */
    show_confirm_overlay(app, "Delete Saved Layout?", info, do_delete);
}
```

`matching_run_gc(app)` cascades — mirrors `clear_window_geometry_for_window`.

### Workspace-lock toggle (`Ctrl+L`)

Flips `LayoutRecord.restore_desktop`. When **on** (default), `apply_window_geometry_restore` honors `record->desktop` (moves window if needed). When **off**, geometry is applied in-place on the current desktop.

`apply_window_geometry_restore` already builds a `GeometryState` plan; adding the conditional skip for `do_desktop`/`do_switch_active_desktop` is ~3 LOC.

### Enable/disable toggle (`Ctrl+T`)

Flips `LayoutRecord.disabled`. When `true`, `apply_window_geometry_restore` short-circuits to no-op (rule still fires, but does nothing). The record remains saved — distinguishes "temporarily off" from "delete".

This is the field we deferred earlier; adding it now per scope expansion.

## LayoutRecord schema additions

```c
typedef struct {
    int match_id;
    int x, y;
    int width, height;
    int desktop;
    bool maximized_vert;
    bool maximized_horz;
    bool fullscreen;
    bool restore_desktop;   /* NEW: default true; if false, restore in-place */
    bool disabled;          /* NEW: default false; if true, apply is no-op */
} LayoutRecord;
```

**Persistence:** the cofi_json_io migration already landed; adding two boolean fields = additive change, tolerant reader returns false for missing keys (back-compat with existing `layouts.json` reading). Writer adds the keys unconditionally.

**Constructors:** `layout_store_set` gets two more args; existing call sites pass `true`/`false`.

## Tests

`test/test_geom_provider.c`:

1. `filter_lists_all_records_with_empty_query`
2. `filter_substring_filters_rows`
3. `row_format_includes_lock_and_disabled_flags`
4. `delete_clears_record_persists_and_triggers_gc`
5. `delete_with_match_entry_still_referenced_by_harpoon_does_not_gc_match` (sam's GC-regression-seam)
6. `delete_shrinks_filtered_count_selection_stays_in_bounds`
7. `lock_toggle_flips_restore_desktop_and_persists`
8. `disable_toggle_flips_disabled_and_persists`
9. `disabled_record_apply_geometry_short_circuits_to_noop`
10. `unlocked_record_apply_geometry_skips_desktop_move`

Plus existing `test_layout_store.c` extended with `restore_desktop`/`disabled` round-trip.

## Renamed command

`:clear-layout` → `:delete-layout` / `:dl`.

- Update `core_commands.c` command spec.
- Update `SPEC.md` lines `:345-346`.
- No back-compat alias — sole user.

## Out of scope

- No CofiPlugin refactor (separate ticket).
- No on-disk corruption recovery UI for orphan LayoutRecords (TBD discussion).
- No rule-linkage display ("which rule auto-fires this layout?") — separate ticket if desired.
- No Matching-tab `[geom]` indicator.
- No bulk operations (delete all unbound, etc.).

## Open question (deferred per user — separate brainstorm)

The shared `MatchEntry` registry shape (1:1 stores keyed by match_id; rules separate as title-pattern selectors) was locked earlier but the user has flagged it for revisit. Outside-the-box brainstorm pending with team about identity model — whether per-feature scoping makes sense, whether rules-being-separate is a smell, etc. Decision may affect future LayoutRecord shape but not this v1 spec.

## ADR follow-up

Write `docs/adr/0011-geom-tab-and-layout-management.md` at landing. Capture:

- Why dedicated tab (not folded into Matching/Windows).
- **Asymmetric GC documented:** geom-delete cascades `matching_run_gc`; matching-delete does NOT clear orphan layouts. Rationale: layouts cheap to regenerate, matching identity precious. Mention this is a known asymmetry, not a bug.
- Why `restore_desktop`/`disabled` fields landed in v1 (deferred earlier; user reopened scope).
- Why no plugin refactor.

## Size estimate

- `geom_provider.c` ~180 LOC (filter inline, 3 action handlers)
- `geom_provider.h` ~10 LOC
- `app_data.h` +2 LOC
- `layout_store.[ch]` +6 LOC (two fields, constructor sig, writer keys)
- `window_geometry_matching.c` ~+10 LOC (apply respects `disabled` + `restore_desktop`)
- `test_geom_provider.c` ~250 LOC (10 tests)
- `core_commands.c` rename ~3 LOC
- `SPEC.md` 1 line

Total: ~470 LOC + tests. One PR.
