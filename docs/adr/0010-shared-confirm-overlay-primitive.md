# 0010 — Shared confirm-overlay primitive (`show_confirm_overlay`)

**Status:** Accepted
**Date:** 2026-05-23
**Supersedes:** none

## Context

Four delete-confirmation overlays (`src/overlay_harpoon.c`, `src/overlay_name.c`, `src/overlay_rules.c`, `src/overlay_sessions.c`) carried near-identical structure: title + info block + `Y`/`Ctrl+D` confirm / `N`/`Esc` cancel. Each maintained its own typed state struct in `AppData` and its own `handle_*_delete_key_press` handler. Drift had set in — instruction text varied (brackets vs no brackets, em-dash vs equals-sign phrasing), `Delete`/`KP_Delete` was accepted only by sessions, and a fifth callsite (`:geom` tab delete, TFD-784) would propagate the pattern further.

The brainstorm cycle initially over-engineered the primitive (`ConfirmOverlaySpec` struct, `gpointer ctx`, structured info pairs, body-render callbacks). The simplifying insight: **the primitive does not need to know what is being deleted**. Each caller already has its module-static state for "the pending thing"; the callback can read that state via module scope. No `ctx` is needed — the C equivalent of a closure is the caller's own static.

## Decision

Single function, four parameters:

```c
void show_confirm_overlay(AppData *app,
                          const char *title,
                          const char *info,
                          void (*on_confirm)(AppData *));
```

- **Confirm keys** (centralized in `src/overlay_confirm.c`): `Y`, `y`, `Ctrl+D`, `Delete`, `KP_Delete`.
- **Cancel keys**: `N`, `n`, `Esc`.
- **Strings**: `g_strdup`'d on show, `g_free`'d on hide — no truncation, no per-overlay sizing.
- **Markup escaping**: caller's responsibility. Both `title` and `info` are rendered as Pango markup; callers `g_markup_escape_text` any user-controlled substrings (window titles, rule patterns, session paths).
- **No `ctx`**: callers store "the pending thing" in module-static state (write-on-show, read-only-in-callback); the callback closes over module scope.
- **Single in-flight overlay**: a second `show_confirm_overlay` while one is active drops the prior `on_confirm` silently (treated as cancel).

The four typed state structs (`harpoon_delete`, `name_delete`, `rules_delete`, `session_delete`) and four `handle_*_delete_key_press` functions were retired in the same PR.

**Behavior change accepted:** `harpoon`/`name`/`rules` delete confirms now accept `Delete`/`KP_Delete` in addition to `Y`/`Ctrl+D`. Today only `sessions` accepted them; default-accepting in the primitive replaces drift with consistency. Acceptable trade in a sole-user project — no muscle-memory regression beyond this contributor's own.

## Consequences

- **One key-handling code path** for all delete confirmations. Future contributors don't reinvent.
- **No drift surface** for instruction text — primitive owns it; callers don't choose phrasing.
- **Caller pattern is universal**: module-static + callback function + `show_confirm_overlay`. Next delete-confirm (e.g. `:geom` tab) is ~20 LOC.
- **Markup escape is a discipline**, not enforced by the API. Caller forgetting `g_markup_escape_text` produces garbled output on hostile titles (e.g. window titled `<script>`). Documented inline in the API contract; future bug-class to watch for.
- **Edit overlays** (`harpoon_edit`, `name_edit`, `name_pattern_edit`, `rules_edit`, `session_rename`) deliberately untouched — different shape (entry field + Enter-commit + validation). Separate primitive when warranted.
- **Cosmetic overlay helpers** (header markup helper, margin/box scaffolding) deliberately not bundled — they repeat across edit overlays too, the right time to extract is when the edit-overlay primitive lands.

## Alternatives considered

- **Per-overlay copy-paste status quo.** Rejected: 4 copies already, geom would make 5; instruction-text drift, `Delete`-key inconsistency, dead duplication of state structs and key handlers.
- **`ConfirmOverlaySpec` struct + `gpointer ctx`.** Rejected: the heap-or-stack lifetime question for `ctx` has no good answer in C; turns a 4-arg call into a 6-field spec + lifecycle ceremony for zero benefit. The primitive doesn't need `ctx` — callers have their own static.
- **`intptr_t ctx_value` by-value.** Rejected as the final form: even an integer ctx is unnecessary; the callback's module already owns the "pending" state. Removing the ctx parameter altogether is the cleaner shape.
- **Fixed `char payload[512]` value-struct.** Rejected: 512 bytes per overlay state for use cases that need 4 bytes; "self-documenting fields" don't justify the cost.
- **Structured `(label, value)[]` info pairs.** Rejected: would impose a uniform info layout but the 4 overlays have legitimately different field counts and emphasis; pre-built markup keeps callers free to format their domain text while the primitive handles the surrounding chrome.
- **Migrate as 4 separate PRs.** Rejected: each migration touches only its own overlay + the primitive call; review attention is fine for one PR; bundling proves the primitive against all 4 shapes in one cycle.
