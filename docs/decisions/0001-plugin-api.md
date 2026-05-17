# cofi plugin API — v2.1

Status: Proposed. This is the live provider API baseline validated by the
ported providers. TFD-675's broader plugin architecture work is tracked in
[0004-plugin-architecture-plan.md](0004-plugin-architecture-plan.md).

## Spike findings (post-migration retrospective)

Four providers ported — calc, sinks, run, proc — in that order, each validating a different API facet. API held without breaking changes to the core types.

### What the numbers actually said

| Provider | Src LOC delta | Notes |
|----------|--------------|-------|
| calc     | ~−120        | calc_mode.c deleted, modal lifecycle to core |
| sinks    | ~−60         | sinks_mode.c absorbed into sinks_provider.c |
| run      | ~−450        | run_mode.c reduced to 3 GTK-free fns |
| proc     | ~+122        | port is NOT always a size win; proc has genuine domain complexity (pipe table, score_row, strict/exact match modes, /proc walk) |

Net across all 4: substantial reduction. But the "port saves LOC" rule only holds when the tab had real glue code to absorb. proc is just complex.

### Boilerplate reality check

~35–40 LOC of true boilerplate per provider: registration block, command-args early-exit. **Not worth abstracting** — registration blocks differ in nearly every field, command-args early-exit has unique post-check per provider. Macro/builder magic would hurt the 6th-tab author more than it helps.

Passthrough adapters (1-liner fns that just forward to the domain fn) are avoidable when domain fn signatures match CofiTabProvider typedefs exactly — assign the fn pointer directly. proc cleaned this up in the final commit (−22 LOC).

### Key tightening absorbed into core during migration

- **Modal lifecycle** (`cofi_modal.c`): prefix claim, Esc-per-policy, on_enter/on_leave fire, entry clear/preserve — fully owned by core (commit 0361cb3). Providers declare `modal_policy`; core does the rest.
- **`suppress_entry_change`** hoisted to `AppData` (was per-RunMode struct) so modals share the guard.
- **Selection preservation** by `row_identity` snapshot+restore: works cleanly across all 4 providers with no per-tab code.
- **Provider display reverse iteration**: cofi's list is INVERTED (idx 0 renders at visible bottom). `format_provider_display` must iterate `end−1 down to scroll`, same as `display_pipeline.c`. This burned 4 iterations on sinks before it was documented — it is the law.

### What surprised us

- **Sinks took 4 ordering iterations**: the inverted-list convention wasn't written down. Now it is.
- **Test coverage gap after run port**: −342 test LOC wasn't all duplicates — real cofi_modal lifecycle coverage, on_selection_changed, Enter-on-empty re-execute paths were missing. Restored in separate commit (+462 LOC). Lesson: count *net* test LOC and validate it covers behavior, not just lines.
- **Duplicate handlers**: `proc_show_handler` and `proc_signal_handler` were byte-identical — only distinguished by `NULL` user_data vs signal int. Caught and merged at final cleanup.

### API verdict

v2.1 is the right stopping point. The contract is validated by 4 real providers. Cross-provider abstraction is not justified. The 6th-tab author needs three files and ~35–40 LOC of wiring — that's the acceptable baseline.

Revised after round-1 review by sam + claudio. Key changes from v1:

- Bool returns replaced with **typed status** (`HANDLED_HIDE`, `HANDLED_KEEP`, `HANDLED_REFRESH`, `NO_OP`, `ERROR`).
- **Filtered/visible vs raw provider index** spelled out explicitly. `selected_idx` always refers to the visible/filtered list.
- **Async generation token** for tick callbacks so stale results can't mutate state.
- `format_row` returns **structured cells** (PID/CPU/MEM/NAME/CMD style) instead of a flat string, so core can right-align columns.
- `aliases` becomes a NULL-terminated pointer list, not a fixed array.
- Added `on_selection_changed` (run mode needs it for Up/Down → entry fill).
- Added **row capabilities flags** (actionable, slottable, error).
- Added **modal_policy** declaration (Esc semantics: return-tab vs hide vs keep-query).
- Added `init_provider_defaults` helper so providers don't need to set every field.
- `on_query_changed` and `pipe_actions` are KEPT in the contract (already validated by proc), even though the migration ports them late.
- Cut `mode_indicator` field — derivable from `prefix_char` (or `display_name` first char if no prefix).

## Goals (unchanged)

- Adding a new list-with-action tab = ~3 source files, not ~13.
- Selection preservation, scroll, modal lifecycle, persistent slots, pipe actions, command-mode integration: all owned by core.
- Existing tabs adopt incrementally; nothing is forced.

## Non-goals (unchanged)

- Dynamic / out-of-process plugins.
- Replacing every tab.
- Action graph / multi-step composition.
- Permission model, sandboxing, hot-reload.

## Types

```c
typedef enum {
    COFI_HANDLED_HIDE,      /* core hides cofi window */
    COFI_HANDLED_KEEP,      /* stay open, CLEAR entry, refresh display (calc/run after exec) */
    COFI_HANDLED_REFRESH,   /* stay open, PRESERVE entry, re-run inventory + re-filter */
    COFI_NO_OP,             /* nothing happened (e.g. unknown action token) */
    COFI_ERROR              /* show transient error row, keep open */
} CofiActionStatus;

typedef enum {
    COFI_ROW_ACTIONABLE = 1 << 0,   /* Enter / pipe action accepted */
    COFI_ROW_SLOTTABLE  = 1 << 1,   /* Ctrl+letter assigns; Alt+letter recalls */
    COFI_ROW_ERROR      = 1 << 2,   /* shown but not actionable; e.g. inventory error */
} CofiRowFlags;

typedef enum {
    COFI_MODAL_HIDE_ON_ESC,           /* Esc → hide tab, return to prefix_origin_tab */
    COFI_MODAL_RETURN_KEEP_QUERY,     /* Esc → return to origin, keep filter text */
    COFI_MODAL_CLEAR_THEN_RETURN,     /* Esc with text → clear; Esc empty → return (calc/run style) */
} CofiModalPolicy;

typedef struct {
    /* up to N cells; core right-aligns numeric, left-aligns string per cell flags */
    struct {
        const char *text;
        int   width_hint;        /* preferred column width; 0 = flexible */
        int   align;             /* 0 = left, 1 = right */
    } cells[8];
    int cell_count;
    int row_flags;               /* CofiRowFlags */
} CofiRowCells;

typedef struct {
    const char *token;            /* "k" */
    const char *aliases[8];       /* {"kill", "term", NULL} — NULL-terminated */
    void       *user_data;        /* opaque per-action payload, e.g. signal int */
    CofiActionStatus (*handler)(AppData *, void *const *payloads, int count, void *user_data);
        /* payloads: array of provider's row identities (always array, even for count=1).
           count: 1 for selected, N for all-visible. */
} CofiPipeAction;

typedef struct {
    const CofiPipeAction *actions;  /* NULL-terminated array */
} CofiPipeActionTable;
```

## Provider struct

```c
typedef struct CofiTabProvider {
    /* identity — required */
    const char *id;                  /* "calc", "sinks", "run", "proc" — also slot_store scope */
    const char *display_name;        /* "CALC", "SINKS", ... */

    /* command surface — optional */
    const char *primary_cmd;         /* ":calc" — NULL = no command surface */
    const char *const *aliases;      /* NULL-terminated; e.g. {"ca", NULL} */
    char        prefix_char;         /* '=', '!' — column-0 entry to claim. 0 = none */

    /* visibility — optional, default 1 */
    int         hidden_by_default;

    /* modal behavior — optional, default COFI_MODAL_HIDE_ON_ESC */
    CofiModalPolicy modal_policy;

    /* list model — required */
    int  (*row_count)(AppData *);                        /* total raw rows */
    void (*format_row)(AppData *, int raw_idx, CofiRowCells *out);
    const char *(*match_string)(AppData *, int raw_idx); /* for fzf scorer */
    const char *(*row_identity)(AppData *, int raw_idx); /* selection preservation key */

    /* filter override — optional */
    int  (*score_row)(AppData *, int raw_idx, const char *query);
        /* return 0 → exclude. higher = better. NULL → core fzf over match_string */

    /* lifecycle — all optional */
    void (*on_enter)(AppData *);
    void (*on_leave)(AppData *);
    void (*on_query_changed)(AppData *, const char *query);
    void (*on_selection_changed)(AppData *, int filtered_idx);
        /* fires on Up/Down/click; run mode uses to fill entry from history row */

    /* periodic tick — optional */
    void (*on_tick)(AppData *, int generation);
        /* generation = monotonically incremented token. Provider passes back to core
           via cofi_tick_apply(generation, ...) when async result returns; core drops
           updates with stale generations. */
    int  tick_interval_ms;            /* 0 = no tick */

    /* primary action — required if any row is actionable */
    CofiActionStatus (*on_enter_pressed)(AppData *, int filtered_idx, int raw_idx, const char *entry_text, int modifier_state);
        /* both indices supplied so provider doesn't need cofi_filtered_to_raw helper */

    /* command-with-args — optional */
    CofiActionStatus (*on_command_args)(AppData *, const char *args);
        /* `:foo bar baz` → on_command_args("bar baz")
           empty args → core surfaces tab, does NOT call this. */

    /* pipe action support — optional */
    const CofiPipeActionTable *pipe_actions;

    /* persistent slot integration — optional */
    int slot_store_enabled;           /* 1 → core wires Ctrl+letter / Alt+letter */
    const char *(*slot_payload_for)(AppData *, int raw_idx);
        /* capture identity to persist (e.g. sink INTERNAL name).
           DISTINCT from row_identity — sinks display "Built-in Audio Pro" but
           slot stores "alsa_output.pci-0000_00_1f.3.pro-output-3".
           No auto-default; provider must supply when slot_store_enabled. */
    CofiActionStatus (*slot_recall)(AppData *, const char *payload);
} CofiTabProvider;

void cofi_init_provider_defaults(CofiTabProvider *p);
    /* memset 0, then set hidden_by_default=1, modal_policy=HIDE_ON_ESC */

int  cofi_register_tab_provider(const CofiTabProvider *);  /* returns TabMode int */
```

## Index semantics — the rule

There are two index spaces:

- **raw_idx**: index into provider's underlying list. Used for `format_row`, `match_string`, `row_identity`, `slot_payload_for`, `score_row`.
- **filtered_idx**: index into the visible/displayed list, after core scoring/sorting/exclusion. Used for `on_selection_changed`, `on_enter_pressed`.

Core maintains the mapping `filtered_to_raw[filtered_idx] → raw_idx` and provides `cofi_filtered_to_raw(filtered_idx)` for providers that need to look up domain data on Enter.

**Pipe actions: handler receives `target_count=1` and a single payload (selected, in filtered space) OR `target_count=N` with an array (all visible, in filtered order, each resolved to its raw payload).** Provider does not see filtered indices in pipe handlers; it gets domain payloads directly.

## What core owns

- `TabMode` enum extension at registration
- Selection state: one `index` + `scroll_offset` per provider, owned by core registry, **not** in `SelectionState` struct
- `selection.c` switch cases: replaced by single dispatch through provider table
- Display rendering: lays out `CofiRowCells` columns, right-aligns numeric, prepends `[a]` slot decoration when applicable
- Modal lifecycle: prefix claim or `:foo` → set indicator (derived), clear input, surface tab, fire `on_enter`
- Esc handling: per `modal_policy`
- Pipe parser: splits `<filter> | <action> [all]`, dispatches via `pipe_actions`, calls handler with resolved payload(s)
- Selection preservation: snapshots `row_identity` before filter, restores after
- `:foo arg` short-circuit via `on_command_args`
- Hide-cofi via `CofiActionStatus`
- Slot store wiring (Ctrl+letter / Alt+letter on tab)
- Tick scheduling: fires only when provider's tab is visible; supplies generation token; drops stale results
- Error rows: when `slot_recall` / `pipe_action` returns `COFI_ERROR`, core appends transient error row (cleared on next refresh or input change)

## What providers own

- Underlying list data
- Domain logic (eval, kill, exec, switch sink)
- Tests for domain logic

## Worked examples

### calc

```c
static const char *const calc_aliases[] = {"ca", NULL};

static CofiTabProvider calc_provider;

void calc_register(void) {
    cofi_init_provider_defaults(&calc_provider);
    calc_provider.id = "calc";
    calc_provider.display_name = "CALC";
    calc_provider.primary_cmd = "calc";
    calc_provider.aliases = calc_aliases;
    calc_provider.prefix_char = '=';
    calc_provider.modal_policy = COFI_MODAL_CLEAR_THEN_RETURN;
    calc_provider.row_count = calc_history_count;
    calc_provider.format_row = calc_format_row;        /* fills 2 cells: result, expr */
    calc_provider.match_string = calc_history_at;
    calc_provider.row_identity = calc_history_at;
    calc_provider.on_enter_pressed = calc_eval_and_push;
    calc_provider.on_command_args = calc_eval_args;
    cofi_register_tab_provider(&calc_provider);
}
```

`calc_eval_and_push`: eval `entry_text` after stripping `=`, push result, copy to clipboard, return `COFI_HANDLED_KEEP` (stay in calc with cleared entry — core handles entry clear on KEEP).

### sinks

Adds slot integration + tick:

```c
sinks_provider.on_tick = sinks_refresh_async;
sinks_provider.tick_interval_ms = 1500;
sinks_provider.slot_store_enabled = 1;
sinks_provider.slot_payload_for = sinks_internal_name_at;
sinks_provider.slot_recall = sinks_set_default_by_name;
sinks_provider.on_command_args = sinks_resolve_alias_or_name; /* :sinks <slot> direct */
```

### run

Adds `on_selection_changed` to fill entry on Up/Down:

```c
run_provider.on_selection_changed = run_fill_entry_from_history;
run_provider.on_enter_pressed = run_execute_or_rerun;
run_provider.modal_policy = COFI_MODAL_CLEAR_THEN_RETURN;
```

`run_execute_or_rerun`: if `entry_text` non-empty → exec; if empty → re-exec selected history row.

### proc

Pipe table + score override:

```c
static const CofiPipeAction proc_pipe_actions[] = {
    {"k", {"kill", "term", "t", NULL}, (void *)(intptr_t)SIGTERM, proc_signal_handler},
    {"9", {"kill9", "force", NULL},    (void *)(intptr_t)SIGKILL, proc_signal_handler},
    {"h", {"hup", NULL},               (void *)(intptr_t)SIGHUP,  proc_signal_handler},
    {"s", {"stop", NULL},              (void *)(intptr_t)SIGSTOP, proc_signal_handler},
    {"c", {"cont", NULL},              (void *)(intptr_t)SIGCONT, proc_signal_handler},
    {"w", {"show", "raise", NULL},     NULL,                       proc_show_handler},
    {NULL}
};
static const CofiPipeActionTable proc_pipe_table = { proc_pipe_actions };

proc_provider.pipe_actions = &proc_pipe_table;
proc_provider.on_query_changed = proc_filter_and_resort;  /* /proc parsing not needed; reuse cached inventory + re-score */
proc_provider.score_row = proc_weighted_score;            /* basename×10 + cmdline, plus `!` and `$` modes */
```

## What does NOT fit

- **Windows tab**: MRU partitioning + X11 event ingestion are product core. Stays as-is. May expose data to slot_store but is not a provider.
- **Harpoon tab**: matcher state, slots bind by class+title. Different mental model. Stays as-is.
- **Names tab**: similar. Stays as-is.
- **Config / Hotkeys / Rules**: edit-form-style, not list-with-action. Stay as-is.

## Apps tab — pressure test

Sam flagged Apps as a possible breaker:
- async desktop-file loading at startup
- app-local ranking (frecency / launch history)
- launch side effects (detached process)
- mixed inventory: desktop apps + PATH binaries (`$` prefix) + system actions

Verdict: **Apps fits IF** core supports two things, both already in v2:
1. Raw vs filtered index discipline (because Apps reorders by frecency, not by the score_row alone) — covered.
2. Inventory loading completes async; `on_tick` with generation token can model "loading… done" — covered.

App's `score_row` does the frecency-aware ranking. `on_query_changed` handles `$` mode-flip to PATH binaries.

Apps porting is a TODO, not a blocker for v1.

## Migration plan (revised)

1. **Add provider infrastructure** — types, registry, dispatch helpers, init helper. No users.
2. **Port calc** — simplest list-with-action. Validates `format_row` cells, `modal_policy`, basic Enter action.
3. **Port sinks** — adds tick + slot_store + command_args. Validates async + persistent state.
4. **Port run** — adds `on_selection_changed`. Validates entry-coupled selection.
5. **Port proc** — adds `pipe_actions` + `score_row` + `on_query_changed`. Validates the action table and weighted scoring.
6. **(Optional) Port apps** — pressure test.

Each port is a separate commit; each one validates a different facet.

## v2.1 patches applied (post-round-2)

- `HANDLED_KEEP` clears entry; `HANDLED_REFRESH` preserves it
- `on_enter_pressed` gets both `filtered_idx` and `raw_idx`
- Pipe handler always `void *const *payloads, int count` (uniform array, no single/all split)
- `slot_payload_for` and `row_identity` are explicitly distinct; no auto-default
- Async result ownership: provider that issues async work owns the payload; if `cofi_tick_apply` is called with stale generation, core ignores the update and provider must free the payload itself
- Error rows: non-slottable (`COFI_ROW_ERROR` flag clears `COFI_ROW_SLOTTABLE`), not preserved across input changes, placed at top of list
- Type/enum/fn naming: `CofiFoo` / `COFI_FOO` / `cofi_foo`

## Resolved questions

(formerly round-2 open questions, now decided)

1. **`format_row` cells max = 8** — arbitrary. Bigger? Variable? Lean on 8 for v1; small enough to stack-allocate.
2. **`on_tick` generation token** — provider has to thread it through async callbacks manually. Footgun? Or is the discipline necessary?
3. **`COFI_HANDLED_KEEP` and entry clearing** — proposed: KEEP also clears the entry (calc style). For tabs that want to keep entry on KEEP, add `COFI_HANDLED_KEEP_PRESERVE_ENTRY`? Or single `KEEP` always clears.
4. **Pipe action handler signature** — `void *target_payload, int target_count`. For `target_count=1`, payload is single ptr; for >1, payload is `void **`. Awkward — better: separate `single_handler` and `all_handler` slots. Reviewer call.
5. **Error rows** — bound to filter result or to inventory? Proposal: bound to inventory, cleared on next successful tick or input change.
6. **slot_recall returns CofiActionStatus** — same as actions. Status maps cleanly.
7. **Apps tab inclusion** — port now or defer? Sam said it fits with raw/filtered discipline. Claudio didn't weigh in.
8. **`row_identity` vs `slot_payload_for`** — when both apply (sinks), are they always the same? Proposal: yes, factor to one fn. Reviewer confirm.
9. **`name aliases` (string-keyed slots)** — does the API cover them via `slot_recall(payload)` lookup keyed by user-provided string instead of letter? Or does it need a sibling `name_store` API? Hold for user spec.
10. **Naming: COFI_ vs cofi_** — uppercase macros, lowercase types/fns, snake_case. Conventional. OK?

## Decision sought from round-2 review

- Are these 10 questions resolvable now, or do any block the migration?
- Anything in v2 that round-1 missed but creates new holes?
- Smallest possible **first port** to validate provider infrastructure before doing all four — calc still the right pick?
