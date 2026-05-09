# cofi plugin shape — evaluation from three spikes

After T1 (prefix-autoswitch), T2 (calc), T3 (sinks), T4 (run), three independent tabs were built directly. This is what actually repeated, what diverged, and what the minimal plugin API has to be.

## What every new tab needed

For each of TAB_CALC / TAB_SINKS / TAB_RUN:

1. **Enum + visibility**: add to `TabMode`, init as hidden, surface on demand.
2. **Selection state**: a `<tab>_index` field + `<tab>_scroll_offset` field on `SelectionState` (8 SelectionState fields per tab × 3 tabs = 24 fields).
3. **Selection switch dispatch** in `selection.c`: ~8 functions × add a case (`init_selection`, `reset_selection`, `get_selected_index`, `move_selection_up/down`, `get_scroll_offset`, `set_scroll_offset`, `update_scroll_position`, `get_total_count_for_tab`).
4. **Display renderer**: a `format_<tab>_display` function in `display.c` plus a switch case in `update_display`.
5. **Tab cycling**: name string + visibility default in `tab_switching.c`.
6. **Modal entry**: enter/exit functions, prefix-strip on entry, mode indicator setting, Esc returns to origin tab. Mirrors `command_mode.c` line by line.
7. **Key handler dispatch**: Enter/Esc/Tab branch in `key_handler.c`, plus a `CMD_MODE_*` case.
8. **Prefix claim**: row in `prefix_tabs.c` mapping prefix char → enter function.
9. **Command triad**: handler (`cmd_<tab>`), forward decl in `_handlers_ui.h`, registration in `command_definitions.h` (primary + aliases + help string).
10. **`:foo arg` short-circuit**: when args present, run the action *without* surfacing the tab; when empty, surface.
11. **Hide cofi window** after a successful action (mirrors Apps).
12. **History/list state**: each tab owns its own data — calc has results, sinks has discovered sinks, run has command history. *This is the actual feature*; everything above is plumbing.
13. **Tests**: behavioral test file plus updates to ~5 cross-cutting test files (visibility, dispatch counts, key-handler integration stubs).

Files touched per tab: ~13 source + ~5 test. Of those, **~10 source files only because they have switch-by-tab dispatch**.

## What diverged (the actual feature surface)

Per tab, only ~3 files are *substantively different*:

- `<tab>.c/h` — domain logic (eval, pactl parse, shell launch)
- `<tab>_mode.c/h` (calc and run only) — modal lifecycle, mostly boilerplate
- `test/test_<tab>.c` — domain tests

Everything else is bookkeeping forced by the switch-by-tab style.

## The shape that emerged

The repeated bookkeeping clusters into five concerns:

1. **List provider**: row count, row at index, match string for fuzzy filter.
2. **Selection model**: index + scroll offset, navigate up/down, reset.
3. **Renderer**: format a row to the display column layout.
4. **Modal lifecycle**: enter (clear input, set indicator, surface tab), Esc semantics, Tab semantics, on-action hide-window.
5. **Command surface**: one canonical id, aliases, args-execute vs args-empty-surface.

These map onto a `CofiTabProvider` struct:

```c
typedef struct {
    const char *id;                  // "calc" / "sinks" / "run"
    const char *display_name;        // "CALC", "SINKS", "RUN"
    const char *aliases[];           // {"ca", NULL} etc.
    char        prefix_char;         // '=', 0, '!'
    const char *mode_indicator;      // "=", "!", or NULL → use default ">"
    int         visibility_default;  // hidden vs pinned

    // List model
    int  (*row_count)(AppData *);
    void (*format_row)(AppData *, int idx, char *out, size_t cap);
    const char *(*match_string)(AppData *, int idx);  // optional

    // Lifecycle
    void (*on_enter)(AppData *);     // surfaced; init list/state
    void (*on_leave)(AppData *);     // tab hidden; teardown timers etc.
    void (*on_query_changed)(AppData *, const char *text);  // optional, for filter

    // Action
    bool (*on_enter_pressed)(AppData *, int selected_idx, const char *entry_text);
                                      // return true → hide cofi
    bool (*on_command_args)(AppData *, const char *args);
                                      // for `:foo bar`; return true → hide, false → fall through to surface

    // Optional
    void (*on_tick)(AppData *);       // periodic; sinks polls here
    int  tick_interval_ms;            // 0 = no tick
} CofiTabProvider;
```

Selection state stops being a switch in `selection.c` — it becomes a single `int index, scroll` pair owned by the tab provider. Display becomes a single `update_display` that dispatches to `provider->format_row`. Modal entry becomes one `enter_provider_mode(app, provider)` instead of three near-copies.

## The honest counter-arguments

- **It's still in-process.** This isn't dynamic-loading; it's just *registration*. That's the right scope (compiled-in is fine for now per user).
- **Three samples may be too few.** Two samples can pattern-match into anything; three is borderline. Watch for the fourth (clipboard? scratchpad?) revealing a missed dimension.
- **Some tabs *aren't* this shape.** Windows tab is deeply tied to X11 events, harpoon tab to persistent slot state. Don't force every tab onto the provider API. Provider is for **list-with-action** tabs.
- **`:foo arg` is the strongest signal.** It's the thing T4 missed and had to be patched. If the API doesn't make this trivial, it failed. The proposed `on_command_args` returning bool gets it right.
- **Modal lifecycle came out three times almost line-identical.** This alone justifies a shared helper, even without the full provider abstraction.

## Decision points for next round

1. **Build the provider abstraction now, port calc/sinks/run to it, and then write a fourth tab against the API to prove it?**
2. Or **stop here**, leave three direct integrations on the spike branch, write up the API shape, and *only* extract when a fourth tab actually arrives?
3. Or **smaller move**: extract just the modal lifecycle (helper) and the selection model (one struct, one set of functions), leave per-tab files otherwise direct. Lowest-risk consolidation.

Recommendation: **option 3 first, option 1 if the fourth tab is concrete and near-term.** Don't build a full plugin API on three spikes if no fourth tab is queued — that's premature.

## Open items captured

- T5 prefix-mode consistency rethink — should fold into provider design if we go that way.
- Cleanup: `plugin-ideas.md` (untracked), stray empty files (`registrations`, `runtime`, `teardown`), `plugin-shape-evaluation.md` (this file). Decide which stay in repo, which are scratch.
- Spike branch: not landed on develop. Decision deferred per "we are spiking for a reason".
