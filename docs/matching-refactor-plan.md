# Matching Subsystem Refactor (foundation for TFD-774)

> Adjusted 2026-05-26 — Foundation A reeval: class/instance/type fields restored as optional with "match if set" semantics. Auto-created entries (`:sl`, harpoon, names) capture full identity. User-authored rules (TFD-808, TFD-809) can leave them empty for title-only matching.

> Revised after design review by sam + claudio. Changes from v1 are marked **[rev]**.

## Why

`TFD-774` (remember window position/workspace) needs a reliable way to say "this
window" across restarts and reopens. That capability already half-exists in the
**Names** subsystem, but it is the wrong shape:

- Names is really a *"stable identity -> current live window"* resolver, but its only
  payoff today is prepending a custom label to a window's title in the Windows tab
  (`filter.c:167`).
- Harpoon is the **same primitive**, copy-pasted (`check_and_reassign_windows` +
  `window_matches_harpoon_slot` vs `check_and_reassign_names` +
  `window_matches_named_entry`). Two near-identical matchers, two rebind loops.
- The user can never author a match *pattern*. The "pattern" is the live title
  frozen at assign time, with a lossy `*`->`.` conversion. So a window that comes
  back with a changed title (`cofi | cofi:develop* - Terminal` ->
  `cofi | cofi:main - Terminal`) fails to rematch.

We refactor Names into a generic **Matching** subsystem that owns window identity,
is shared by all consumers, and lets the user generalise a title into a pattern once
— benefiting every consumer (harpoon today, geometry next).

## Locked model

Three id-like things, each with a distinct role (today they are wrongly collapsed
onto the X11 window id):

| Thing | Role | Lifetime | Held by |
|---|---|---|---|
| `match_id` | stable cross-subsystem key | persisted forever | matching registry; referenced by every consumer |
| match criteria | class/instance/type (exact) + title + `match_mode` | persisted, **user-editable** | data inside the match entry |
| `bound_x11_id` | live binding / fast path / disambiguator | **persisted, validated on load** | match entry |

```
match entry (matching.json):
  match_id        stable key, persisted
  class/instance/type   exact-match criteria
  title           the literal title (capture) OR an authored pattern
  match_mode      EXACT (default) | GLOB     [rev]
  label?          optional, secondary
  bound_x11_id    persisted, but validated by class/instance/type on load   [rev2]

consumer record:
  harpoon: { match_id, slot }
  geom:    { match_id, x, y, w, h, desktop }   (later, = TFD-774)
  label:   lives on the match entry itself
```

### Invariants

- **One match entry per identity.** Two entries for the same window would desync
  criteria + binding. Forced, not optional.
- **Capture dedups.** On harpoon-assign or geom-save: find the entry whose live
  `bound_x11_id == this window` **(only if that binding is still live)**, else whose
  criteria match this window **by EXACT title** (never by GLOB — see below), else
  create a new entry. Skip entries already bound to a *different* live window.
- **`match_id` is the persistent key, not the criteria.** Editing title/pattern must
  not orphan consumers — they reference `match_id`, which is stable.
- **`bound_x11_id` is the disambiguator, and IS persisted but validated on load.**
  **[rev2]** A cofi restart is not an X restart — windows keep their ids, so a
  persisted id gives an instant exact rebind across `mise run restart` and is the
  only thing that disambiguates *which* of N identical windows held a slot/geom.
  Hazard (id reused after a full X/server restart) is neutralised by **load-time
  validation against `class`+`instance`+`type` only (NOT title)**: find the live
  window at the persisted id; if its class/instance/type match the entry, trust the
  binding (this survives title drift — same window, new branch in title); otherwise
  drop the id and rebind via pattern. Residual edge accepted for prototype: id reused
  across X-restart by a *different* window of the *same* class binds to a sibling;
  low harm, user re-assigns.
- **Exact by default.** Capture sets `match_mode = EXACT` and stores the literal
  title. Matching is plain `strcmp` on the title — no wildcard interpretation, so a
  real `?`/`*` in a title is harmless. Generalising is an explicit edit in the
  Matching tab that flips the entry to `match_mode = GLOB`. **[rev]** This removes the
  literal-vs-wildcard escaping problem entirely.
- **Delete semantics.** **[rev]** Deleting a *label* clears the label but preserves
  the entry while any consumer (harpoon/geom) still references its `match_id`. An
  entry is removed only when it has no label and no consumer references. (Prototype:
  a simple reference check across harpoon + geom is enough; no refcount field.)

## Pattern language **[rev — do not touch `wildcard_match`]**

`wildcard_match` (`window_matcher.c:57`) is also used by **rules** (`rules.c:31`,
`rules_replay.c:20`) with `.`=single-char semantics. **Changing it is a silent rules
regression.** So:

- Add a **new** `glob_match(pattern, str)` entry-point: `?` = exactly one char,
  `*` = any run (incl. empty). Leave `wildcard_match` and rules untouched.
- `match_mode = EXACT` -> `strcmp(title, live_title) == 0`.
- `match_mode = GLOB` -> `glob_match(title, live_title)`.
- class / instance / type stay exact `strcmp` in both modes.
- Authoring a literal `?`/`*` inside a GLOB pattern (escaping) is **out of scope** —
  punt until it bites. Captured EXACT entries never need it.

## Decomposition (each PR independently shippable + reviewable)

Test discipline per `CLAUDE.md`: **characterise before refactor.** Each structural PR
starts by adding/confirming behavioural tests that pass on current code, then refactors
with them green.

### PR1 — Consolidate the matcher (behaviour-preserving) **[rev]**
- Characterisation tests pinning current harpoon + named match behaviour first.
- Extract a single shared match core in `window_matcher.c`; both
  `window_matches_harpoon_slot` and `window_matches_named_entry` call it.
- Add the new `glob_match()` function (with `?`/`*` unit tests) but **do not wire it
  into any consumer yet** and **do not modify `wildcard_match`/rules.**
- **No semantics change to existing matching, no rename, no schema change.** Pure
  mechanical consolidation + a new dormant helper. Independently shippable.

### PR2 — Introduce `match_id`, split id/binding, capture-dedup **[rev]**
- Split the conflated `NamedWindow.id`/`assigned`: add stable `match_id` (persisted);
  `bound_x11_id` is **persisted but validated on load** (find live window at the id;
  trust only if class/instance/type match — see model invariants). Preserve the
  existing orphan-then-rebind dance (`named_window.c:137`) against the new fields.
- Tests: persist+reload binding survives a simulated cofi restart (same id, drifted
  title -> still bound); reused id with different class -> dropped + rebound.
- `match_id` allocation: a persisted `next_match_id` counter; never reuse a freed id.
  Tests: create/delete/reload, malformed/duplicate ids.
- Add `match_mode` field, default `EXACT`. `EXACT` matches title via `strcmp`; `GLOB`
  via `glob_match`. **Drop the `*`->`.` capture conversion HERE** (store the literal
  title), atomically with the EXACT/strcmp switch — NOT in PR1, where matching still
  uses `wildcard_match` and dropping it would be a behaviour change. Update the
  characterisation tests that pinned "asterisk replaced by dot" accordingly.
- Capture-dedup helper — **spec the signature**:
  `int matching_capture_or_get(AppData *app, const WindowInfo *w);`
  returns an existing `match_id` (live-bound to `w`, else EXACT-title match) or
  creates one; returns `-1` if the registry is full. Skip stale/other-bound entries;
  dedup tiebreak = first entry in registry order (deterministic).
- Cover the **third call site** `app_init.c:102/196` (`load_named_windows` +
  `check_and_reassign_names` run at startup, not only in `x11_events.c`). **[rev]**
- Tests: same window captured twice -> one entry; stale-id re-resolve; overlapping
  EXACT vs (future) GLOB.

### PR3a — Mechanical rename Names -> Matching (zero behaviour change) **[rev split]**
Rename only; no new UI. Full list (from review):
- tab id `names`->`matching`, display `NAMES`->`MATCHING`.
- command `:names/:nm` -> add `:matching/:m` (**`:m` confirmed free; alias is `:m`,
  not `:m?`**). Keep `:names` as a back-compat alias.
- source files: `names_provider`->`matching_provider`, `named_window`->`match_entry`,
  `named_window_config`->`match_entry_config`, `filter_names.c`->`filter_matching.c`,
  `overlay_name.c` (see PR3b).
- `AppData` fields `app->names` (`NamedWindowManager`) + `app->filtered_names[]` ->
  matching equivalents. **Header change -> full rebuild required** (warn CI).
- `COFI_OPCODE_NAMES`: **keep the numeric byte (4)** and route it to Matching; rename
  the symbol + the `"names"` string in `daemon_socket.c:31` carefully
  (`daemon_socket.h`, `daemon_socket.c`, `daemon_socket_runtime.c`, `cli_args.cpp`).
- CLI: add `--matching`, **keep `--names` as an alias** (no breakage). `cli_args.cpp:62`
  help text.
- docs/specs: `SPEC.md`, `docs/adr/` opcode docs (ADR-0002), `core_commands.c` `:show`
  help, `command_handlers_ui.c`, `--help` / `:help` surfaces, tests, fixtures.
- `names_row_identity` -> `"match:{match_id}"` so selection survives rebinds. **[rev]**

### PR3b — Matching-tab UI: pattern primary, label secondary **[rev split]**
- **Decision: Option B.** `:an` (assign name) stays **label-only** — it keeps prompting
  for a custom label, and capturing also silently creates/dedups the EXACT match entry.
  Pattern authoring is a SEPARATE `Ctrl+P` action in the Matching tab. Do NOT repurpose
  `:an` to prompt for the pattern.
- Matching tab shows the **title/pattern** as the primary column; label optional.
- New pattern-edit action (`Ctrl+P`) -> edits the title and flips entry to
  `match_mode = GLOB`. `Ctrl+E` keeps editing the label. This is a non-trivial GTK
  overlay (`overlay_name.c`-sized) — either extend it or add `overlay_match_pattern.c`.
- `filter.c` label-prepend keeps working via `match_id`/`bound_x11_id` lookup; the
  rebind loop must run **before** `filter_windows()`.
- Manual test gate: name a window, author `cofi*Terminal`, change its title across a
  branch switch, confirm it still rematches.

### PR4 — Harpoon references `match_id` **[rev — full surface]**
Harpoon slot stores `{ match_id, slot }` instead of cloning criteria. Touch points
(from review — bigger than v1 stated):
- assign-slot -> `matching_capture_or_get`.
- activation: `get_slot_window()` / Alt recall resolve `match_id -> bound_x11_id`
  (fast path) or re-resolve.
- `get_window_slot()` (Windows-tab slot indicators), `filter_harpoon()`, Harpoon tab
  rows, edit/delete overlays, `serialize_window_slot_payload()`.
- **`HarpoonManager.store` is shared across providers** (Projects/Sinks/etc). Only
  migrate the *window* harpoon payloads; do not break other providers' slots.
- **Legacy load: drop/reset (clean slate).** `load_legacy_harpoon_slots` can be
  removed or short-circuited; do not migrate old harpoon payloads. Single user —
  re-assigning slots after the change is acceptable.
- Remove the dead duplicate harpoon matcher/rebind once parity tests pass.
- Manual test gate: harpoon a window, change its title, confirm the slot still
  activates it; restart cofi, confirm slots survive.

### PR5 (separate; the actual TFD-774) — Geometry payload **[rev]**
- New geom payload keyed by `match_id` (own section/table).
- Capture action: **not `:ls`** (taken by harpoon list-slots). Use a Matching-tab key
  (e.g. `Ctrl+G`) or a command like `:save-geom`. Pick concretely.
- Stores `get_window_geometry` + `_NET_WM_DESKTOP` via `matching_capture_or_get`.
- **Apply policy — default: explicit-invocation only**, not on every rebind (else
  cofi fights manual tiling/drag on every title change). A toggle (per-entry flag or
  `config.json`) can enable apply-on-rebind later. State the default in the PR.
- Workspace restore is a separate, independently-toggleable step.

## Rename fallout checklist (PR3a) **[rev]**

`filter_names.c`, `overlay_name.c`, `app_init.c` call sites, `app_data.h` fields,
`COFI_OPCODE_NAMES` (4 files, keep byte), `--names`/`--matching` (`cli_args.cpp`),
`daemon_socket.c` opcode string, `names_row_identity` format, `SPEC.md`,
`docs/adr/` (ADR-0002), `core_commands.c`, `command_handlers_ui.c`, help surfaces,
tests, fixtures, and any user hotkeys using `show names` / `:names` (kept as alias).

## Migration / reset **[rev3 — clean slate]**

Single user, prototype. **No migration.** Assume a clean slate:

- `matching.json` is a new schema written fresh. Old `names.json` / harpoon JSON may
  be ignored or overwritten; stale config can simply be deleted. Do **not** build
  best-effort loaders or legacy auto-migration.
- PR4 legacy-harpoon path: drop it / reset, don't migrate.
- `bound_x11_id` IS persisted and **validated on load** by class/instance/type (see
  model invariants).

## Out of scope (this refactor)
- Geometry beyond PR5 wiring (multi-layout, predefined layouts, global
  always-restore) — later TFD-774 iterations.
- Literal `?`/`*` escaping in authored GLOB patterns.
- Changing `wildcard_match` / rules semantics.
- TFD-775 tiling-quality work.
</content>
