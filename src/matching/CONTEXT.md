# Matching

## Purpose
Matching owns pure window identity records and title/anchor matching primitives
that higher layers use to find, restore, name, and target windows.

## Boundary

### Owns
- Fuzzy scoring primitives for query-to-text matching, including the legacy
  matcher and the fzf-style scorer used by providers.
- Persistent `MatchEntry` identities: stable match ids, live X11 rebinding,
  title patterns, class/instance/type anchors, and JSON persistence in
  `matching.json`.
- The hidden Matching provider tab for read-only browsing of identity entries.
- Pure match-entry garbage collection against a caller-provided list of live
  match ids.

### Does Not Own
- X11 window enumeration, activation, geometry, workspace, or event primitives;
  matching consumes `WindowInfo` snapshots from x11.
- Provider-specific row formats, actions, command semantics, or slot payloads
  outside the Matching provider itself.
- Custom display names and their persistence; those belong to `src/names/`.
- Windows-tab filtering, display-title composition, and alt-tab selection;
  those belong to `src/ui/`.
- Cross-owner matching GC root collection; that belongs to `src/core/app/`.
- Rule execution semantics, saved-layout application, or Harpoon slot recall;
  matching only supplies stable identities and edited patterns to those
  consumers.
- Generic overlay hosting, confirmation UI, tab switching, command mode, or
  display rendering infrastructure.

## Public Surface
- `has_match()`, `match()`, `match_positions()`, `fzf_has_match()`, and
  `fzf_fuzzy_match()`
- `MatchEntry`, `MatchEntryManager`, `match_entry_manager_init()`,
  `match_entry_is_bound_window()`, `match_entry_reassign_live_windows()`,
  `match_entry_gc()`,
  `match_entry_get_by_index()`, `match_entry_find_index_by_window()`,
  `match_entry_find_index_by_match_id()`,
  `match_entry_matches_window()`, `match_entry_delete_by_match_id()`,
  `matching_create_entry()`, and `matching_create_pattern_entry()`
- `save_match_entries()` and `load_match_entries()`
- `windows_match_fuzzy()`, `titles_match_fuzzy()`,
  `get_title_base_length()`, and `wildcard_match()`
- `matching_provider_register()`, `matching_tab_mode()`,
  `matching_selected_entry()`, and `matching_selected_manager_index()`

## Acceptance Criteria
1. Direct word-boundary matches rank in a higher tier than indirect fuzzy
   subsequence matches, regardless of workspace bonus.
2. Indirect fuzzy matches are capped below the direct tier, while consecutive
   adjacent word-start matches receive the acronym-style bonus used by ranking
   tests.
3. `has_match()` and `fzf_has_match()` are case-insensitive subsequence checks;
   empty fzf needles match, while empty legacy needles do not produce a useful
   ranking score.
4. `match()` returns `SCORE_MAX` for equal-length case-insensitive matches,
   `SCORE_MIN` for empty needles, too-long haystacks, or impossible matches, and
   otherwise ranks by boundary/consecutive/gap bonuses.
5. `match_positions()` returns the same score as `match()` and writes one
    position per needle character along an optimal match path when a positions
    buffer is supplied.
6. `fzf_fuzzy_match()` returns `SCORE_MIN` for null inputs, needles longer than
    haystacks, or inputs above the fzf length cap, and returns non-negative fzf
    scores for valid matches.
7. Creating a match entry assigns a never-reused positive `match_id`, captures
    class, instance, type, live X11 id, and title pattern, and escapes literal
    `*` in captured titles to `.` so source titles do not become broad
    wildcards. After populating fields, the entry is self-verified against its
    source window via `match_entry_matches_window()`; a mismatch logs a WARN but
    the entry is kept.
8. Pattern-only entries have no class/instance/type anchors, are unassigned,
    and always get a fresh stable id even when another entry has the same
    pattern. Consumers that own pattern-only identities, such as rules, keep
    ownership one-to-one by storing their own match id; geom-tagged restore
    rules do not use this path and instead reuse the saved layout's anchored
    match id as the geom restore identity.
9. Match-entry window matching requires the title pattern to match and every
    non-empty class, instance, and type anchor to match exactly.
10. Live rebinding keeps an existing bound X11 id while it is present; only dead
    bindings are cleared and searched for a replacement by pattern/anchors.
11. Rebinding may bind multiple match entries to the same live window when
    their independent owner identities match the window; uniqueness is by
    `match_id`, not by live X11 id.
12. Match entries are pure identity records and never own custom display names;
    names are stored in `names.json` by `src/names/`.
13. Deleting a match entry by index or stable match id compacts later entries,
    clears the vacated tail slot, never reuses the removed match id, and treats
    a missing match id as a no-op.
14. Loading `matching.json` initializes an empty manager first, tolerates missing
    files, restores saved entries up to `MAX_WINDOWS`, and normalizes missing or
    duplicate match ids to fresh positive ids.
15. Saving matching writes `next_match_id` and every entry's id, binding,
    pattern, anchors, and assigned state through the shared JSON persistence
    helper; legacy `custom_name` fields are ignored on load and not written.
16. `match_entry_gc()` preserves entries whose match id appears in the
    caller-provided referenced-id list, removes only unreferenced entries, and
    reports the number of removed entries without collecting roots itself.
17. The Matching provider is hidden by default, registers only the
    `matching`/`m` command, filters rows by pattern, anchors, or type, and
    resets selection on query changes.
18. Matching provider rows show pattern, class, instance, type, and live binding
    status; row identity is `match:<match_id>`.
19. The Matching provider is read-only: it ignores edit, delete, and pattern
    edit keys, and only supports browsing/filtering identity rows.
20. Name assignment is owned by `src/names/`; matching only allocates or updates
    the identity entry requested by that subsystem.
21. Name editing and deletion are owned by `src/names/`; name deletion may
    delete that name record's owned match entry, while name editing must not
    mutate `matching.json`.
22. `wildcard_match()` treats `*` as any sequence, `.` as exactly one character,
    and all other characters as exact, case-sensitive matches.
23. Fuzzy window identity requires class, instance, and type equality before
    applying title fuzziness; title fuzziness accepts exact matches, shared
    dash-prefix bases, or one title containing the other.

## Notes
- Matching ids are the stable cross-subsystem key. Do not replace them with
  array indexes in rules, Harpoon, geom, or provider rows.
- The legacy matcher and fzf-style matcher both remain public because different
  providers and tests rely on their exact scoring behavior.
