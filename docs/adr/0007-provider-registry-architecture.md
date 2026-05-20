# 0007 — Registry-based tab architecture (CofiTabProvider)

**Status:** Accepted
**Date:** 2026-05-20
**Supersedes:** none — graduates the design-exploration notes in [docs/decisions/](../decisions/) (0001–0004) into a decision record.

## Context

Every non-Windows tab used to spread its logic across ~13 files: `TAB_*` switch cases in display, selection, key dispatch, plus hardcoded command registration and daemon-opcode handling. Adding a tab meant editing all of them. The plugin-API exploration (`docs/decisions/0001-plugin-api.md` … `0004-plugin-architecture-plan.md`, TFD-675) designed a single registration interface, validated by porting calc → sinks → run → proc (each commit validating a different facet), then completed across all remaining list tabs (apps, names, rules, config, hotkeys, harpoon, workspaces, sessions/projects) and the new emoji tab.

## Decision

- Every non-Windows tab registers a `CofiTabProvider` struct (`src/cofi_tab_provider.h`) via `builtin_plugins.c`. The `TAB_*` switch cases and per-tab arrays are gone.
- **Core owns:** display rendering of structured `CofiRowCells`, the shared render pipeline + scrollbar (`render_display_pipeline`), selection (snapshot/restore by `row_identity`), modal lifecycle + Esc policy (`cofi_modal.c`), persistent slots, command-mode integration (`CommandSpec`), daemon-opcode dispatch (provider-metadata-driven), and tab enablement (fail-closed registry lookup, TFD-719).
- **Providers own:** their list data, domain logic, and filtering via `on_query_changed` building a local filtered-index array.
- The **Windows tab stays core** (MRU partitioning + X11 event ingestion are product core, not a list-with-action provider). Compiled-in only; no out-of-process / dynamic plugins.

## Consequences

- A new list-with-action tab is ~3 files / ~35–40 LOC of wiring, not ~13 files.
- **Provider-owned self-filtering is the canonical path.** `match_string` is in the contract but currently **unused** — reserved for a future central scorer; filtering happens in each provider's `on_query_changed`.
- **`row_count` / `format_row` index through the provider's own filtered set**, not raw data; `on_enter_pressed` maps the filtered index back through it. (raw vs filtered index discipline.)
- **Inverted-list render convention is law:** index 0 renders at the visible *bottom*; providers and `render_display_pipeline` iterate `end−1 → start`. (Burned four iterations before it was written down.)
- The unified render pipeline (all providers through `render_display_pipeline` + shared `overlay_scrollbar`) and provider enablement (TFD-719) are **consequences of this decision**, not separate records.
- `docs/decisions/0001–0004` become historical exploration; this ADR is the canonical record.

## Alternatives considered

- **Keep `TAB_*` switch-cases** — status quo; didn't scale, every tab touched the core in ~13 places.
- **Two-binary / out-of-process plugins** — rejected as an explicit non-goal (no sandboxing/hot-reload need; compiled-in keeps the single-binary daemon model from [0002](0002-unix-socket-opcode-daemon.md)).
- **Macro/builder abstraction over registration blocks** — rejected: registration structs differ in nearly every field; the abstraction would hurt the next author more than the ~35 LOC it saves.
- **Central filter over `match_string`** — deferred, not removed; providers self-filter today and `match_string` stays a reserved hook.
