# Core Utilities

## Purpose
Core utilities provide shared, product-neutral helpers used by multiple cofi
subsystems: safe string copying, shortcut parsing and canonicalization, UTF-8
display-column measurement and fitting, shared constants and data types, fzf
bonus vocabulary, and the embedded arithmetic expression evaluator.

## Boundary

### Owns
- General-purpose helpers that do not belong to a product feature.
- Shared constants, small data types, and standardized `CofiResult` error codes.
- Shortcut parsing and hotkey canonicalization used by configuration, overlays,
  daemon hotkey flows, and tests.
- UTF-8 display-column measurement, cleaning, clipping, fitting, alignment, and
  ellipsis helpers for rendered text.
- The header-only fzf bonus lookup vocabulary consumed by matching.
- The vendored `tinyexpr` evaluator used by calculator behavior.

### Does Not Own
- JSON persistence helpers; those live in `core/json`.
- Logging policy, log sinks, or user-facing diagnostics.
- Window, workspace, X11, provider, command, or overlay behavior.
- Feature-specific configuration schemas or saved-state formats.
- Filesystem path, config-directory, or cache-directory discovery.

## Public Surface
- `safe_string_copy()` copies optional input into caller buffers and always
  null-terminates when the destination has capacity.
- `parse_shortcut()` parses shortcut strings into GDK keyvals and modifiers.
- `parse_shortcut_with_error()` does the same and writes caller-facing
  diagnostics on failure.
- `canonicalize_hotkey_shortcut()` rewrites user-entered shortcuts into the
  canonical XGrabKey string format.
- `canonicalize_hotkey_event()` converts live `GdkEventKey` captures into the
  same canonical shortcut format.
- `utf8_cluster_columns()` returns display columns for one UTF-8 cluster.
- `utf8_next_cluster()` advances to the next cluster boundary without splitting
  combining marks, emoji modifiers, ZWJ sequences, or flag pairs.
- `utf8_text_columns()` measures a full string in display columns.
- `utf8_clean_text()` normalizes text for single-line display.
- `utf8_fit_columns()` truncates and pads text to an exact display width.
- `utf8_fit_columns_aligned()` adds optional right alignment.
- `utf8_fit_columns_ellipsis()` truncates with dots when text does not fit.
- `utf8_clip_lines_to_columns()` clips each line of a `GString` in place.
- `types.h` provides shared size limits, `ShowMode`, and forward declarations
  for `WindowInfo`, `WorkspaceInfo`, `HarpoonSlot`, and `HarpoonManager`.
- `constants.h` provides display-width constants, match-tier scoring constants,
  indicators, `DEFAULT_FONT`, and `CofiResult`.
- `bonus.h` provides `bonus_states`, `bonus_index`, `COMPUTE_BONUS`, and the
  `ASSIGN_LOWER` / `ASSIGN_UPPER` / `ASSIGN_DIGIT` table macros.
- `tinyexpr.h` provides `te_variable`, `te_expr`, `te_interp()`,
  `te_compile()`, `te_eval()`, `te_print()`, and `te_free()`.

## Acceptance Criteria
1. `safe_string_copy()` leaves no unterminated destination buffer when the
   destination pointer is non-null and the destination size is greater than zero.
2. `parse_shortcut()` and `parse_shortcut_with_error()` accept the supported
   modifier aliases and key aliases used by cofi configuration, including
   Ctrl/Control, Alt/Mod1, Super/Mod4/Win/Windows, Shift, common named keys,
   punctuation aliases, function keys, and numpad aliases.
3. Invalid shortcut strings, unknown modifiers, unknown keys, modifier-only key
   tokens, empty input, or null required arguments fail without producing a
   partially valid shortcut; the error-producing variant writes a bounded
   diagnostic when a buffer is supplied.
4. `canonicalize_hotkey_shortcut()` emits modifiers in Control, Shift, Mod1,
   Mod4 order, rejects unsupported X11-grab modifiers such as Meta and Hyper,
   rejects invalid keys, and is idempotent for already canonical shortcuts.
5. `canonicalize_hotkey_event()` rejects missing events, missing output buffers,
   and modifier-only key events; valid captures round-trip through
   `canonicalize_hotkey_shortcut()` into the canonical hotkey string format.
6. Empty or null string input produces a null-terminated empty output where the
   helper owns an output buffer, returns zero columns for measurement helpers,
   or returns the documented failure/sentinel result without buffer over-read.
7. UTF-8 measurement is based on display columns, not bytes: combining marks,
   variation selectors, emoji modifiers, ZWJ clusters, flag pairs, and wide
   characters contribute the same widths callers render against.
8. UTF-8 traversal and cluster-width helpers advance safely over invalid UTF-8
   byte sequences without reading past the input, treating invalid bytes as
   single-column replacement units for width calculations.
9. `utf8_clean_text()` collapses Unicode whitespace, control characters,
   linebreaks, and invalid-byte runs into single spaces while trimming
   leading/trailing spaces.
10. `utf8_fit_columns()` never splits a UTF-8 cluster and pads the output to the
    requested display-column width when the output buffer has capacity.
11. `utf8_fit_columns_aligned()` right-aligns only when `align_right` is true;
    otherwise it delegates to the left-aligned fitting behavior.
12. `utf8_fit_columns_ellipsis()` truncates with a literal `...` for widths
    greater than three; target widths of three or less output only dots and no
    clipped content.
13. `utf8_clip_lines_to_columns()` clips each line independently and preserves
    line breaks while avoiding partial UTF-8 clusters.
14. `CofiResult` values remain stable as public error-code contracts:
    success is zero, and all standardized error cases are negative.
15. `bonus_states[]` and `bonus_index[]` define the fzf boundary-bonus lookup:
    previous/current character classes deterministically select slash, word,
    capital, dot, or zero bonus for every byte pair.
16. `COMPUTE_BONUS` and the `ASSIGN_*` macros produce deterministic table
    entries for lower-case letters, upper-case letters, and digits without
    caller-side branching.
17. `te_compile()` reports a nonzero error position, or `-1` when no syntax tree
    can be built, on parse failure.
18. `te_interp()` returns `NAN` on parse or evaluation failure rather than
    crashing callers.
19. Compiled tinyexpr expressions support variable binding through
    `te_variable` and can be reused with `te_eval()` until released with
    `te_free()`.

## Notes
`utils.c` shortcut parsing is intentionally shared by user configuration and
runtime hotkey flows. Changes in this area should be checked against
`docs/gotchas.md` before altering behavior.

UTF-8 display-column behavior is covered by
`docs/adr/0008-utf8-column-rendering.md`.

`bonus.h` includes `config/config.h` for the `SCORE_MATCH_*` constants and uses
the `score_t` vocabulary expected by matching callers.

`types.h` currently includes `<X11/Xlib.h>` even though it only provides shared
limits, enums, and forward declarations; every consumer of `types.h` gets X11
headers transitively.

`canonicalize_hotkey_event()` builds intermediate strings with `Alt+` and
`Super+`, while `canonicalize_hotkey_shortcut()` emits `Mod1+` and `Mod4+`.
The roundtrip is intentional today but fragile if alias handling changes.

`tinyexpr` is vendored third-party code. Treat edits there as a library upgrade,
not as ordinary cofi utility changes.
