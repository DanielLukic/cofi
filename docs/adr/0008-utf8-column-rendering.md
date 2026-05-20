# 0008 — Cluster-correct UTF-8 column rendering

**Status:** Accepted
**Date:** 2026-05-20 (`828974a`)

## Context

cofi was intentionally ASCII-only: column-width arithmetic throughout the display assumed 1 byte = 1 column (`strlen`-based padding/clipping), documented as an invariant in `CLAUDE.md`. The emoji picker tab (and UTF-8 window titles in general) broke that assumption — emoji, CJK, ZWJ sequences (`👨‍💻`), regional-indicator flags (`🇩🇪`), skin-tone modifiers (`👍🏽`), and variation-selector emoji misalign every column to their right when measured in bytes.

## Decision

- All display-column arithmetic goes through `src/utf8_columns.c` (`utf8_text_columns`, `utf8_fit_columns[_aligned/_ellipsis]`, `utf8_clip_lines_to_columns`), which measures in **grapheme-cluster display columns**, not bytes. `strlen` is forbidden for display sizing.
- A cluster containing **VS16** (`U+FE0F`) counts as **2 columns** (emoji presentation) — e.g. `❤️` (U+2764 U+FE0F) is 2 even though `❤` alone is 1.
- **Scope is display only.** Fuzzy match targets stay ASCII: only the rendered glyph is non-ASCII; search strings are names/keywords. Searching window titles *by* UTF-8 input is explicitly deferred.

## Consequences

- New cross-cutting invariant (in [architecture.md](../architecture.md)): **no `strlen` for display-column arithmetic — use `utf8_*`.**
- `overlay_scrollbar` was reworked to measure and clip by display columns (was byte-based) so the scrollbar column survives the final clip and never splits a cluster.
- Window titles and rows with emoji/CJK/ZWJ/flags now align correctly; the emoji tab renders.
- The matcher is untouched and stays ASCII-oriented; the **display-glyph vs match-string split** is an intentional exception to [0004](0004-fzf-fuzzymatchv2-full-row.md)'s "display order = search order" — the glyph cell is non-ASCII but is never the search target.

## Alternatives considered

- **Keep ASCII-only; strip/replace emoji on display** — kills the emoji-tab goal and still mis-renders ZWJ/CJK window titles.
- **`g_unichar_iswide` only** (already used in the old `clip_display_lines_to_columns`) — visually correct only for basic CJK; misses ZWJ sequences, skin-tone modifiers, and VS16 width promotion.
- **ICU** — external dependency with no precedent in this codebase; far more than needed for column measurement.
