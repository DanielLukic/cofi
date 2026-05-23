# 0009 — Tolerant JSON I/O via `cofi_json_io`

**Status:** Accepted
**Date:** 2026-05-23
**Supersedes:** none

## Context

cofi persists ~12 stores under `~/.config/cofi/` (window matching, layouts, harpoon slots, rules, hotkeys, options, calc history, emoji history, sessions, browser profiles, projects, slot store). Four (`calc.c`, `sessions.c`, `browser_profiles.c`, `projects_remote_store.c`) used `json-glib`. The other eight (`layout_store.c`, `slot_store.c`, `harpoon_config.c`, `hotkey_config.c`, `emoji_provider.c`, `rules_config.c`, `match_entry_config.c`, `config.c`) hand-rolled JSON via `fprintf`/`sscanf`.

The hand-rolled half had two real problems:

- **Field-order-strict parsing.** `layout_store.c` used `sscanf` on a 6-field positional template; any reorder broke reads. Adding a field meant rewriting the parser.
- **No shared tolerance policy.** Each store decided independently what "missing field" / "wrong type" / "corrupt file" meant. Subtle divergence with no enforcement.

Each new field carried silent-breakage risk. We had been adding fields (geom v1 grew `LayoutRecord` from 5 fields to 9) and would continue to. The right time to address this was before the next field add — and before the next persistence-touching feature (`:geom` tab, TFD-784) propagated more hand-rolled code.

## Decision

Introduce a thin tolerant-JSON wrapper over `json-glib`:

- **`src/cofi_json_io.[ch]`** — 8 functions: `cofi_json_load_object_file`, `cofi_json_save_root`, typed-getter trio (`_str_or` / `_int_or` / `_bool_or` with optional `present` out-param), and lookup pair (`_obj_array` / `_obj_object` returning NULL on missing/wrong-type).
- **Tolerance policy codified in the helpers** (see [Cross-cutting invariants](../architecture.md#cross-cutting-invariants)) and asserted by `test/test_cofi_json_io.c` (15 cases). Unknown fields ignored; missing/wrong-type returns fallback with `present == FALSE`; corrupt/non-object root → NULL + `log_error`.
- **Atomic save:** write `<path>.tmp` in the same directory, then `rename()`. Target untouched on any failure. No `fsync` — accepted v1 trade-off.
- **Pilot migration:** `layout_store.c` save/load swapped to the wrapper; `calc.c` reader ported to the typed getters (writer was already json-glib correct). Both consumers used in one PR proves the API against two different shapes (scalar-fields-in-array vs string-field-objects) before the API locks for the remaining migrations.

What's deliberately **not** in the wrapper:

- **No descriptor DSL / field-table mapping.** Per-store mapping stays grep-readable. A registry of `(field_name, type, default, offset)` was rejected — it would become a second schema language.
- **No `schema_version` field yet.** Sole user; future field-removal is not yet on the table. Add when first needed; field-or-default already covers additive compatibility.
- **No `fsync`, no journaling.** tmp+rename is the v1 atomicity guarantee.
- **No object-builder helper.** `JsonBuilder` from json-glib is adequate; the wrapper would add noise. Revisit after the third migration if it becomes obvious.

## Consequences

- **One tolerance policy.** Future contributors see one place for what "missing field" means.
- **Field-reorder resilience.** sscanf-strict ordering retired with the `layout_store` pilot; same applies to every subsequent migration.
- **One-time reformat per store** on first save after migration (json-glib's whitespace and escape conventions differ from hand-rolled output). Files aren't user-grepped in practice; CHANGELOG notes this.
- **String lifetime discipline required.** Returned `const char *` is owned by the parser tree; callers must copy if storing past parser lifetime. Documented inline in `cofi_json_io.h`.
- **Remaining migrations queued** (one PR each, ordered by risk): harpoon → rules → match_entry → emoji → slot_store → hotkey → config. Each independently shippable; coexistence with hand-rolled stores is fine — the wrapper imposes no global cutover.
- **The four stores already on json-glib** (`sessions`, `browser_profiles`, `projects_remote_store`, and `calc` now via the wrapper) remain untouched for now; they may adopt helpers opportunistically when changed for other reasons.

## Alternatives considered

- **Status quo (hand-rolled fprintf/sscanf per store).** Rejected: known correctness debt; field-order-strict parsing already a regression risk; no shared tolerance.
- **Direct `json-glib` per store, no wrapper.** Rejected: codifies the `has_member ? get_string : default` boilerplate that already existed in the 4 json-glib stores. The wrapper *names* an existing pattern, not a new one.
- **Descriptor DSL / field-table.** Rejected during design review (claudio + sam): would become a second schema language, harder to migrate to than the per-store helpers it replaces.
- **`schema_version` from the start.** Rejected: "looks free, gates a conversation" — no decided upgrade policy yet. Add when first removing a field.
- **Byte-stable output.** Rejected: trying to match hand-rolled formatting constrains the wrapper API for negative payoff. Accept one-time reformat.
- **`fsync` for atomicity.** Deferred: tmp+rename is already a substantial improvement over fopen+fprintf; mid-write crash protection is a future concern (TFD-781 follow-up if it ever bites).
- **One umbrella PR migrating all 8 stores.** Rejected: dwarfs review attention, blocks geom work, no real benefit over per-store cutover.
