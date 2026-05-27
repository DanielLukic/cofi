# Matching

## Purpose
Matching turns window text, saved identities, and user-edited patterns into the
stable rows and scores cofi uses to find, name, restore, and target windows.

## Boundary

### Owns
- Fuzzy scoring primitives for query-to-text matching, including the legacy
  matcher and the fzf-style scorer used by providers.
- Window-list filtering for the Windows tab, including display-title
  composition, match tiers, workspace biasing, and selection handoff.
- Persistent `MatchEntry` identities: stable match ids, live X11 rebinding,
  custom names, title patterns, class/instance/type anchors, and JSON
  persistence in `matching.json`.
- The hidden Matching provider tab for browsing/editing named entries.
- Name and pattern edit overlays for matching entries, including refresh and
  persistence of affected matching, rules, geom, and Harpoon views.
- Matching garbage collection across Harpoon slots, custom names, saved
  layouts, and rules references.

### Does Not Own
- X11 window enumeration, activation, geometry, workspace, or event primitives;
  matching consumes `WindowInfo` snapshots from x11.
- Provider-specific row formats, actions, command semantics, or slot payloads
  outside the Matching provider itself.
- Rule execution semantics, saved-layout application, or Harpoon slot recall;
  matching only supplies stable identities and edited patterns to those
  consumers.
- Generic overlay hosting, confirmation UI, tab switching, command mode, or
  display rendering infrastructure.

## Public Surface
- `has_match()`, `match()`, `match_positions()`, `fzf_has_match()`, and
  `fzf_fuzzy_match()`
- `filter_windows()` and `apply_alt_tab_selection()`
- `compose_window_display_title()`
- `MatchEntry`, `MatchEntryManager`, `match_entry_manager_init()`,
  `match_entry_assign_custom_name()`, `match_entry_get_custom_name()`,
  `match_entry_is_bound_window()`, `match_entry_reassign_live_windows()`,
  `match_entry_collect_labeled_ids()`, `match_entry_gc()`,
  `match_entry_delete_custom_name()`, `match_entry_update_custom_name()`,
  `match_entry_get_by_index()`, `match_entry_find_index_by_window()`,
  `match_entry_find_index_by_match_id()`,
  `match_entry_find_index_by_custom_name()`,
  `match_entry_matches_window()`, `matching_create_entry()`, and
  `matching_find_or_create_pattern_entry()`
- `save_match_entries()` and `load_match_entries()`
- `windows_match_fuzzy()`, `titles_match_fuzzy()`,
  `get_title_base_length()`, and `wildcard_match()`
- `matching_run_gc()`
- `matching_provider_register()`, `matching_tab_mode()`,
  `handle_matching_tab_keys()`, `matching_selected_entry()`,
  `matching_selected_manager_index()`, and `matching_select_custom_name()`
- Name and pattern overlay creation/key handlers declared in
  `overlay_name.h` and `overlay_pattern.h`

## Acceptance Criteria
1. Empty window queries preserve the current window order, expose every current
   window in `filtered`, and apply the configured alt-tab selection policy.
2. Non-empty window queries score against the same display string users see:
   desktop label, display instance/class, and title with any custom name prefix.
3. Direct word-boundary matches rank in a higher tier than indirect fuzzy
   subsequence matches, regardless of workspace bonus.
4. Indirect fuzzy matches are capped below the direct tier, while consecutive
   adjacent word-start matches receive the acronym-style bonus used by ranking
   tests.
5. Workspace bias promotes current-workspace windows only within compatible
   score tiers; it must not let a weaker tier outrank a stronger direct match.
6. Filtering records the selected window id after sorting so later refreshes can
   preserve selection when that window is still present.
7. Display-title composition prefixes a custom name as
   `<custom name> - <window title>` and otherwise returns the original title.
8. `has_match()` and `fzf_has_match()` are case-insensitive subsequence checks;
   empty fzf needles match, while empty legacy needles do not produce a useful
   ranking score.
9. `match()` returns `SCORE_MAX` for equal-length case-insensitive matches,
   `SCORE_MIN` for empty needles, too-long haystacks, or impossible matches, and
   otherwise ranks by boundary/consecutive/gap bonuses.
10. `match_positions()` returns the same score as `match()` and writes one
    position per needle character along an optimal match path when a positions
    buffer is supplied.
11. `fzf_fuzzy_match()` returns `SCORE_MIN` for null inputs, needles longer than
    haystacks, or inputs above the fzf length cap, and returns non-negative fzf
    scores for valid matches.
12. Creating a match entry assigns a never-reused positive `match_id`, captures
    class, instance, type, live X11 id, and title pattern, and escapes literal
    `*` in captured titles to `.` so source titles do not become broad
    wildcards. After populating fields, the entry is self-verified against its
    source window via `match_entry_matches_window()`; a mismatch logs a WARN but
    the entry is kept.
13. Pattern-only entries have no class/instance/type anchors, are unassigned,
    reuse an existing identical pattern when present, and get a new stable id
    only when no identical pattern entry exists.
14. Match-entry window matching requires the title pattern to match and every
    non-empty class, instance, and type anchor to match exactly.
15. Live rebinding keeps an existing bound X11 id while it is present; only dead
    bindings are cleared and searched for a replacement by pattern/anchors.
16. Rebinding never assigns two custom-name entries to the same live window; a
    candidate already bound to another named entry is skipped.
17. Custom-name assignment updates an existing matching entry for the selected
    window identity when possible, otherwise creates a new entry and stores the
    requested name.
18. Deleting a match entry compacts later entries, clears the vacated tail slot,
    and never reuses the removed match id.
19. Loading `matching.json` initializes an empty manager first, tolerates missing
    files, restores saved entries up to `MAX_WINDOWS`, and normalizes missing or
    duplicate match ids to fresh positive ids.
20. Saving matching writes `next_match_id` and every entry's id, binding,
    custom name, pattern, anchors, and assigned state through the shared JSON
    persistence helper.
21. Matching garbage collection preserves entries referenced by Harpoon slots,
    custom names, saved layouts, or rules, removes only unreferenced match ids,
    and saves matching when anything was removed.
22. The Matching provider is hidden by default, registers the
    `matching`/`m`/`names`/`nm` command, filters rows by name, pattern, anchors,
    or type, and resets selection on query changes.
23. Matching provider rows show pattern, custom name, class, instance, type, and
    live binding status; row identity is `match:<match_id>`.
24. `Ctrl+E` opens the selected matching row for custom-name editing, `Ctrl+P`
    opens pattern editing for the selected match id, and `Ctrl+D` asks for
    delete confirmation before removing a name.
25. Name assignment is allowed only from the Windows tab with a selected window;
    empty input cancels without saving, and successful assignment saves matching
    then refreshes the active window filter.
26. Name editing and deletion resolve the current Matching provider selection
    back to the manager by stable `match_id` or custom-name fallback, save
    matching, refilter the tab, clamp selection, and refresh display.
27. Pattern editing refuses missing or orphaned targets with an explanatory
    confirmation overlay; successful edits save matching and refresh the active
    Matching, Rules, Geom, or Harpoon view as appropriate.
28. When a pattern edit affects rules or saved layouts, the dependent persisted
    rules config and geom-tagged restore rules are resynchronized for the old
    and new patterns.
29. `wildcard_match()` treats `*` as any sequence, `.` as exactly one character,
    and all other characters as exact, case-sensitive matches.
30. Fuzzy window identity requires class, instance, and type equality before
    applying title fuzziness; title fuzziness accepts exact matches, shared
    dash-prefix bases, or one title containing the other.

## Notes
- Matching ids are the stable cross-subsystem key. Do not replace them with
  array indexes in rules, Harpoon, geom, or provider rows.
- Pattern editing intentionally reaches into rules and geom persistence because
  those records store matching ids but user-visible restore behavior is pattern
  based. Keep that bridge explicit and one-way from the edit action.
- The legacy matcher and fzf-style matcher both remain public because different
  providers and tests rely on their exact scoring behavior.
