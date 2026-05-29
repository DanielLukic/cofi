# Rules

## Purpose
Rules let users bind saved window identities to cofi command strings so matching
windows can trigger actions automatically, be replayed manually, and be managed
from a hidden provider tab.

## Boundary

### Owns
- The persisted rules list in `~/.config/cofi/rules.json`, including pattern
  cache, stable `match_id` references, command strings, startup flags, once
  flags, new-window-only flags, and optional subsystem tags.
- Rule evaluation policy: resolving a rule through `MatchEntry`, tracking
  per-rule/per-window transition state, enforcing the `once` flag, and returning
  command strings to dispatch.
- In-memory rule state stored on `AppData`: `RuleState`, `RuleBreakerState`,
  filtered rule rows, filtered config indexes, and `Rule.applied` bookkeeping.
- The Rules provider tab, including filtering, row formatting, keyboard
  shortcuts, replay actions, once toggling, and provider command registration.
- Rule add/edit/delete overlays and validation of comma-separated command
  strings against the command registry before persistence.
- Manual replay of selected or all rules against currently open windows.

### Does Not Own
- Match-entry creation, wildcard matching semantics, or fuzzy matching
  primitives; rules consume identities and match helpers from `matching/`.
- Command execution semantics, command parsing rules, or command availability;
  rules validate and dispatch through the command subsystem.
- X11 window enumeration, title-change events, dead-window pruning triggers, or
  event-loop dispatch; x11 invokes rules and supplies live `WindowInfo` data.
- Geometry restore planning or saved-layout definitions; geom may tag rules and
  sync restore rules, but rules only stores/evaluates the resulting records.
- Generic overlay hosting, provider registry internals, tab switching, or display
  rendering.

## Public Surface
- `Rule`, `RulesConfig`, `MAX_RULES`, `MAX_PATTERN_LEN`,
  `MAX_COMMANDS_LEN`, and `MAX_RULE_TAG_LEN`
- `init_rules_config()`, `save_rules_config()`, `load_rules_config()`,
  `add_rule()`, `remove_rule()`, and `rule_commands_contain_segment()`
- `RuleTrigger`, `RuleState`, `RuleWindowState`, `RuleBreakerState`, `RuleMatch`,
  `init_rule_state()`, `check_rule_match()`, `rule_matches_window()`,
  `rule_trigger_allows()`, `rule_toggle_once()`, `rule_toggle_new_only()`,
  `rules_clear_applied_for_dead_windows()`, `rule_state_remove_window()`,
  `init_rule_breaker()`, `rule_breaker_should_fire()`, and
  `rule_state_prune_absent()`
- `replay_rule_against_open_windows()`,
  `replay_all_rules_against_open_windows()`, and
  `replay_selected_filtered_rule()`
- `rules_provider_register()`, `rules_tab_mode()`,
  `handle_rules_tab_keys()`, `filter_rules()`, `rules_selected_rule()`,
  `rules_selected_config_index()`, and `rules_select_config_index()`
- `create_rule_add_overlay_content()`, `create_rule_edit_overlay_content()`,
  `handle_rule_add_key_press()`, `handle_rule_edit_key_press()`, and
  `show_rule_delete_confirm()`

## Acceptance Criteria
1. `init_rules_config()` produces an empty rules list, and `add_rule()` appends
   at most `MAX_RULES` entries with copied pattern/commands, `run_at_start`
   false, no tag, `once` true, and `applied` cleared.
2. `remove_rule()` rejects invalid indexes, compacts later rules in order when
   removing a valid index, and decrements the count.
3. Saving rules writes `rules.json` with each rule's `match_id`, current pattern
   cache, command string, `run_at_start`, `once`, `new_only`, and non-empty tag;
   only `applied` state is transient and never persisted.
4. When saving with a matching manager, a rule with a valid `match_id` writes the
   matched entry's current original title as the persisted pattern cache.
5. Loading missing, corrupt, or malformed `rules.json` never crashes callers:
   missing files load as an empty success, corrupt JSON resets to an empty
   config, and invalid rule objects are skipped.
6. Loading legacy pattern-only rules migrates each valid non-empty pattern to a
   `MatchEntry` and stores the resulting positive `match_id`; legacy rules that
   cannot be migrated are skipped.
7. Loading a rule whose saved `match_id` is orphaned falls back to a non-empty
   saved pattern by creating or reusing a pattern entry; otherwise the rule is
   skipped.
8. Loaded rules restore command strings, `run_at_start`, `once`, `new_only`,
   and tags, default missing `once` to true, missing `new_only` to false, and
   missing tags to empty, clear `applied`, and normalize the visible pattern
   from the referenced match entry when available.
9. Rule matching resolves through `MatchEntry`: a rule with no positive
   `match_id`, no manager, no window, or a missing entry does not match.
10. `check_rule_match()` tracks transition state by `(rule_index, window_id)` so
    different rules evaluating the same window do not stomp each other's matched
    flag.
11. A rule fires only on the no-match to match transition for a given
    `(rule_index, window_id)`: it does not re-fire while the window remains
    continuously matched. With `once == false`, the rule may fire again after a
    non-match clears the matched state and the same window re-enters the
    pattern. With `once == true`, the first transition stores the applied window
    id and later transitions remain suppressed until `applied` is cleared.
12. A title change away from a rule clears the per-window matched flag, but a
    once-applied rule remains suppressed until the applied-window bookkeeping is
    cleared by the dead-window path.
13. A `new_only` rule may fire only when rule evaluation is triggered by a
    `_NET_CLIENT_LIST` addition for that newly-added window; startup scans,
    existing-window client-list refreshes, and title changes do not fire it.
14. Manual replay bypasses the automatic trigger gate, so `new_only` rules can
    still be replayed manually against matching open windows.
15. `rules_clear_applied_for_dead_windows()` clears each rule's `applied` id
    only when that id is absent from the live X11 window list supplied by x11.
16. `rule_state_prune_absent()` removes transition-state entries for windows not
    present in the live list and keeps entries for still-live windows.
17. The circuit breaker rate-limits independently by `(rule_index, window_id)`,
    allows up to ten fires per one-second burst, suppresses the eleventh fire,
    and rearms after more than two seconds of quiet.
18. The circuit breaker fails open when no breaker state is supplied or when the
    breaker entry table is full.
19. `rule_toggle_once()` flips the selected rule's `once` flag and always clears
    `applied` so the new mode starts from an unapplied state;
    `rule_toggle_new_only()` flips only `new_only` and leaves `applied`
    unchanged.
20. Manual replay evaluates the chosen rule against every currently open window,
    dispatches matching command strings through `execute_command_background()`,
    respects `once`/`applied`, and does not mutate `RuleState` transition flags.
21. Replaying all rules walks `RulesConfig.rules` in stored order and includes
    tagged rules even when the provider list would hide them.
22. `rule_commands_contain_segment()` matches only complete comma-separated
    command segments after trimming whitespace; substrings such as `url`, `rlx`,
    or `foorl` do not satisfy an `rl` lookup.
23. The Rules provider is hidden by default, registers the `rules` and `rs`
    commands, surfaces the dynamic Rules tab, and keeps cofi open for hotkey
    auto mode.
24. Entering the Rules tab sets the placeholder to `Type to filter rules...`
    and filters all visible rules into rows with a leading flags cell; each
    query change refilters and resets provider selection.
25. Provider filtering searches the resolved match-entry pattern plus command
    string and hides tagged rules unless `config.rules_show_all_tags` is true.
26. Provider rows use three cells: flags, the resolved match-entry original
    title or `<cached pattern> (orphan)`, and command string. The flags cell
    renders `O` for once, `Ø` when once is applied to a live window, `N` for
    `new_only`, concatenates set flags, and renders `-` when no flags are set;
    empty lists expose one non-actionable `No rules found` row.
27. Provider row identity is `rule:<match_id>:<commands>`, and selected rows map
    back to `RulesConfig.rules[]` through `filtered_rule_indices`, not copied row
    indexes alone.
28. In the Rules tab, `Ctrl+A` opens Add, `Ctrl+E` opens command edit for the
    selected row, `Ctrl+P` opens pattern edit for the selected match id,
    `Ctrl+D` opens delete confirmation, `Ctrl+O` toggles once and persists it
    without moving selection, `Ctrl+N` toggles `new_only` and persists it
    without moving selection, `Ctrl+X` replays the selected rule, and
    `Ctrl+Shift+X` replays all rules.
29. Add Rule requires a non-empty pattern and command string, validates every
    comma-separated command against the command registry, creates or reuses the
    pattern match entry, saves both matching and rules, then refreshes the Rules
    tab.
30. Edit Commands changes only the command string for the selected config rule;
    it keeps the pattern and `match_id`, validates commands, saves rules, and
    refreshes the list.
31. Delete confirmation displays escaped pattern and command text, removes the
    pending config rule on confirmation, saves rules, refilters, clamps
    selection, and refreshes display.

## Notes
- `Rule.applied` is mutable evaluation state embedded in `RulesConfig` because
  once-mode suppression needs to survive across event callbacks, but only the
  `once` flag is persisted; `applied` is not.
- The x11 event path owns when rule evaluation happens and when dead-window
  pruning runs. Rules only provides the state transitions those callbacks invoke.
- Geom-tagged rules are ordinary rule records with a tag and the layout's
  anchored `match_id`; keep the geom bridge explicit so rules does not grow
  geometry-planning ownership.
