# Names

## Purpose
Names owns user-assigned custom labels for window identities. It maps a stable
`match_id` to one persisted display name and lets the Windows and Names tabs
show, search, edit, and delete those labels without making `MatchEntry` carry
presentation data.

## Boundary

### Owns
- Persistent name records in `names.json`, keyed one-to-one by `match_id`.
- Assigning a name to the current visible window identity, including creating a
  match entry only when no existing name record matches that window.
- Resolving a window's display name by matching live window title and anchors
  against name-owned match entries.
- The Names provider tab, including filtering, row identity, edit/delete
  actions, and name overlays.
- Name match ids as matching-GC roots.

### Does Not Own
- Match-entry identity fields, match-id allocation, live rebinding, pattern
  matching, or matching JSON persistence.
- Harpoon slots, saved layouts, rules, or their ownership of match ids.
- Window enumeration, activation, geometry, or X11 property handling.
- Generic command parsing, provider registration mechanics, overlay hosting, or
  display rendering infrastructure outside name-specific rows and overlays.

## Public Surface
- `NameRecord`, `NamesStore`, `names_store_init()`,
  `names_store_init_with_path()`, `names_store_load()`,
  `names_store_save()`, `names_store_set()`,
  `names_store_get_by_match_id()`, `names_store_find_index_by_match_id()`,
  `names_store_find_by_custom_name()`, `names_store_remove_by_match_id()`,
  and `names_store_collect_ids()`
- `names_assign_window()` and `names_get_for_window()`
- `names_provider_register()`, `names_tab_mode()`, `names_on_query_changed()`,
  `handle_names_tab_keys()`, `names_selected_record()`,
  `names_selected_store_index()`, and `names_select_custom_name()`
- Name overlay creation/key handlers declared in `overlay_names.h`

## Acceptance Criteria
1. Store initialization creates a default path at
   `$HOME/.config/cofi/names.json`, creating parent directories as needed, and
   tests can override the path explicitly.
2. `names_store_set()` rejects missing stores, non-positive match ids, and empty
   names; otherwise it inserts or updates exactly one record for the match id.
3. Saving writes `{ "names": [ { "match_id": N, "custom_name": "..." } ] }`
   and does not write match-entry identity data.
4. Loading initializes an empty store first, tolerates missing or corrupt files,
   skips malformed records, caps at `MAX_WINDOWS`, and keeps unknown JSON fields
   ignored.
5. Lookup by `match_id` returns the stored custom name, lookup by custom name
   returns the first matching record, and remove-by-match-id compacts later
   records and clears the vacated tail.
6. `names_store_collect_ids()` returns positive match ids in store order up to
   the caller-provided capacity so app-level matching GC can treat names as live
   roots.
7. `names_get_for_window()` iterates name records, resolves each record's match
   entry by `match_id`, and returns the first whose entry matches the live
   window title and anchors; it must not resolve by stale bound X11 id alone.
8. Assigning a name to a window first resolves an existing matching name record
   using the same current-title/anchor logic as display lookup; when found, it
   updates that record's name and binding without creating another match entry.
9. Assigning a name with no matching name record creates one current-title match
   entry, stores one name record for that match id, and saves both matching and
   names persistence.
10. Display-title composition prefixes a resolved custom name as
    `<custom name> - <window title>` and otherwise returns the original title.
11. The Names provider is hidden by default, registers the `names`/`nm` command,
    filters rows by custom name, pattern, anchors, or type, and resets selection
    on genuine query changes.
12. Names provider rows show custom name, pattern, class, instance, type, and
    live binding status; row identity is `name:<match_id>`.
13. `Ctrl+E` opens the selected Names row for custom-name editing, `Ctrl+P`
    opens matching pattern editing for that name record's `match_id`, and
    `Ctrl+D` asks for delete confirmation before removing the name record.
14. Name editing updates only `names.json`; name deletion removes only the
    `NameRecord`, saves names, runs matching GC, refilters with
    preserve/restore selection, and refreshes display.
15. Names may import matching to resolve identity records and allocate match ids;
    matching identity code must not depend on names.

## Notes
- Names is a first-class owner of match ids. Each name record owns exactly one
  match id, and that match id may be distinct from Harpoon, geom, or rules
  entries that also happen to match the same live X11 window.
- App-level `matching_run_gc()` collects name roots through
  `names_store_collect_ids()`; the lower-level matching subsystem does not
  import names.
- Legacy `custom_name` fields in `matching.json` are intentionally ignored; no
  migration is performed.
