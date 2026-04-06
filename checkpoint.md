# Cofi Development Checkpoint — 2026-04-06 (updated)

Intent: this file is the current-state checkpoint for active development. It tracks branch state, recent completed work, known issues, and operational context needed to resume quickly. It is not the canonical workflow guide and not the long-term home for cross-cutting gotchas.

See also:
- `CLAUDE.md` for contributor workflow, branching rules, and repo conventions
- `docs/gotchas.md` for fragile behaviors, regressions to avoid, and implementation learnings
- `SPEC.md` for intended product behavior

## Repo & Infra

- **Repo**: https://github.com/DanielLukic/cofi
- **Owner**: DanielLukic (transferred from IntensiCode)
- **Default branch**: `develop` (changed from `main` — 2026-03-31)
- **Tracking**: Linear via `linearis`
- **Team**: `TFD` / `IntensiCode` (`e82a4779-dde1-44e0-9772-c86ad681114f`)
- **Project**: `Cofi - The Comfortable Window Switcher` (`6478f7c1-ab2b-4842-ba97-74db69fed434`)
- **Known issue range**: TFD-76 to TFD-101
- **Systemd service**: `~/.config/systemd/user/cofi.service`
- **Restart**: `./restart.sh` — does `make clean && make`, restarts service, shows last log lines
- **Logs**: `journalctl --user -u cofi -f`
- **Linear CLI**: `linearis`

## Critical Rules (in CLAUDE.md and memory)

1. **NEVER push to develop/main/release without explicit user consent**
2. **NEVER create merge commits — always clean rebase**
3. **Direct session work goes straight to develop (no PR needed)**
4. **Subagent work uses worktrees + PRs targeting develop**
5. **App logic PRs need user testing before merge; trivial (docs/dead code) can merge immediately**
6. **After any header (.h) change: `make clean && make` required** (deps via `-MMD -MP`)
7. **Build → commit → restart after every feature/fix so user can test**
8. **"Ticket" = Linear board card, not just an issue**
9. **Subagents must always branch from `develop`**

## Current Branch State

Branch: `develop`. All work since last push is here.

### Commits not yet on origin/main (newest first):

```
2314d06 Add window title to hotkey conflict dialog
7387dc2 Fix :tm closing cofi and remove duplicate activate_window
237a139 Add minimize window command (:miw / :min)
fbd60b0 Fix scrollbar: pad lines to window width, place at rightmost column
0b1c5bd Add push/merge rules to CLAUDE.md: no unauthorized pushes, rebase only
a8bf625 Add live hotkey re-grab after bind/unbind
e14f095 Add case-insensitive hotkey parsing, aliases, and typo suggestions
15e0e0e Add header dependency tracking to Makefile (-MMD -MP)
9182349 Update docs to match codebase: 6 tabs, 6-column display, missing commands
e02bb48 Update CLAUDE.md: branch workflow, subagent PR rules, current architecture
3ab1ee3 Remove dead format_config_display() after :config switched to Config tab
bc915de Add window_order_mode config, fix column-first slot sort, add restart script
```

(Last fully pushed baseline: `0aa26f8 Make :cfg/:config switch to interactive Config tab`)

## Recently Completed Work (this session)

### Hotkey conflict dialog title (2314d06)
- Added `gtk_window_set_title(GTK_WINDOW(dialog), "Cofi — Hotkey Conflict")`
- File: `src/hotkeys.c` in `show_grab_failure_dialog()`

### Fix :tm crashing cofi (7387dc2)
- Root cause: `gtk_main_quit()` was baked into `move_window_to_next_monitor()` since
  original commit `0ecd091` when `:tm` was dispatched inline. Never removed when
  `cmd_toggle_monitor` handler was introduced.
- Fix: removed `gtk_main_quit()` from `move_window_to_next_monitor()`
- Also removed duplicate `activate_window()` call — `cmd_toggle_monitor` already calls
  `activate_commanded_window(app, window)`
- File: `src/monitor_move.c`

### :miw / :min — Minimize Window (237a139)
- New command `cmd_minimize_window` in `command_mode.c`
- Toggles: minimizes if not hidden, restores via `activate_window` if already minimized
- Uses `get_window_state(display, id, "_NET_WM_STATE_HIDDEN")` to detect minimized state
- Uses `XIconifyWindow` to minimize; `activate_window` / `XMapRaised` to restore
- Added `minimize_window()` to `x11_utils.c` / `x11_utils.h`
- Aliases: `miw`, `min`, `minimize-window` (no compact form)
- 3 alias tests added to `test_command_aliases.c`
- SPEC.md and README.md updated

### Scrollbar fix (fbd60b0)
- `overlay_scrollbar()` signature: added `target_columns` parameter
- Pads each line to `max(content_width + 2, window_columns)` — +2 for space + scrollbar char
- `get_display_columns()` added to `dynamic_display.c` — measures text view width / char width
- All 7 callers updated (6 in `display.c`, 1 in `command_mode.c`)
- 26 unit tests in `test/test_scrollbar.c` (NOT yet in run_tests.sh — see Known Issues)

### Live hotkey re-grab (a8bf625)
- `regrab_hotkeys(AppData*)` = `cleanup_hotkeys` + `setup_hotkeys`
- Called after `:hotkeys` bind/unbind and Hotkeys tab Ctrl+D delete
- No restart needed for hotkey changes

### Hotkey key name UX (e14f095)
- Case-insensitive modifier parsing (ctrl/Ctrl/CTRL all work)
- 11 modifier aliases: ctrl, control, shift, alt, mod1, super, mod4, win, windows, meta, hyper
- 28 key aliases: enter/return, esc/escape, del/delete, pageup/page_up, arrows, punctuation
- Levenshtein-based "did you mean?" suggestions on typos
- `parse_shortcut_with_error()` new function; old `parse_shortcut()` preserved as wrapper
- 69 unit tests in `test/test_parse_shortcut.c` (NOT yet in run_tests.sh — see Known Issues)

### Window order mode + column sort (bc915de)
- `window_order_mode` config: "cofi" (MRU) or "native" (_NET_CLIENT_LIST_STACKING Z-order)
- Two-phase gap-based column sort replaces old `x/50` bucket approach:
  Phase 1: sort by X → Phase 2: assign columns by X-gap (threshold 50px) → Phase 3: sort (col, Y)

### Config tab / :cfg (0aa26f8 — already on origin)
- `:config`/`:cfg`/`:conf` exits command mode and switches to interactive Config tab
- `:set key value` applies and switches to Config tab
- Old static overlay removed

### log_level config (e24fce4 — already on origin)
- `log_level` in options.json, default "debug"
- CLI `--log-level` override; `:set log_level info` applies at runtime
- Ctrl+T cycles in Config tab

### Linear tracking
- Linear is accessed from the `linearis` CLI, not via MCP tooling
- Useful commands:
  `linearis teams list`
  `linearis projects list`
  `linearis issues read TFD-82`
  `linearis issues list -l 25`

## Linear Tickets (Cofi project)

### Open / Backlog
| ID | Title | Notes |
|----|-------|-------|
| TFD-82 | Hotkey capture mode | ~90 lines. Needs: temporary ungrab during capture, GDK key event → "Mod1+Tab" format conversion. Low priority. |
| TFD-85 | External config screen | Someday/maybe. Lua/guile/python external config editor. |
| TFD-100 | Overhaul window width/height management | Window height driven by Windows tab, doesn't adapt for :help. Width measurement unreliable on first display. Need fixed/static approach. |

### Closed this session
- TFD-101: Minimize window command (:miw)
- TFD-76 to TFD-99: Historical issues (all Done, migrated from GitHub)

## Test Suite

**498 tests** currently run by `make test` across 10 test files:

| File | Count |
|------|-------|
| test_command_parsing | 47 |
| test_config_roundtrip | 32 |
| test_config_set | 46 |
| test_hotkey_config | 46 |
| test_fzf_algo | 46 |
| test_named_window | 55 |
| test_match_scoring | 36 |
| test_command_aliases | 115 |
| test_wildcard_match | 49 |
| test_command_dispatch | 26 |

**NOT yet in run_tests.sh (need to add):**
- `test/test_parse_shortcut.c` — 69 tests
- `test/test_scrollbar.c` — 26 tests

Run all: `make test`

## Key File Map

| File | Purpose |
|------|---------|
| `src/main.c` | Entry point, key handlers, tab switching |
| `src/display.c` | Text formatting, overlay_scrollbar, tab formatters |
| `src/dynamic_display.c` | Font metrics, line count, get_display_columns |
| `src/config.c` | Load/save/apply, build_config_entries (single source of truth) |
| `src/command_mode.c` | All `:` commands incl. cmd_minimize_window, cmd_toggle_monitor |
| `src/command_definitions.h` | Command table (primary, aliases, handler, description) |
| `src/command_parser.c` | parse_command_and_arg, compact forms (cw5, jw4 etc) |
| `src/monitor_move.c` | move_window_to_next_monitor — moves only, no activation/quit |
| `src/workspace_slots.c` | Per-workspace slot assignment, two-phase column sort |
| `src/hotkeys.c` | XGrabKey registration, regrab_hotkeys, show_grab_failure_dialog |
| `src/hotkey_config.c` | hotkeys.json load/save |
| `src/x11_utils.c` | X11 property extraction, activate_window, minimize_window |
| `src/filter.c` | Search/scoring, MRU vs native ordering |
| `src/utils.c` | parse_shortcut with aliases and typo suggestions |
| `src/history.c` | MRU ordering, partition_and_reorder |
| `restart.sh` | clean build + systemd restart + log tail |
| `Makefile` | `-MMD -MP` header deps, `make install` for systemd, `make test` |
| `SPEC.md` | Authoritative behavior specification |

## Known Issues / Tech Debt

1. **`test_parse_shortcut` and `test_scrollbar` not in `run_tests.sh`** — add them
2. **Window width/height management** (TFD-100) — window height driven by Windows tab content,
   doesn't adapt for `:help`. Width measurement unreliable on first display.
3. **`src/monitor_move.c` unused parameter warning** — `screen` param in
   `move_window_to_next_monitor_with_screen()` is unused (GDK monitor detection TODO)
4. **Unused includes in `monitor_move.c`** — `display.h`, `window_list.h`, `filter.h`
   flagged by clangd (not build failures)

## Architecture Notes

- **Command dispatch**: `command_definitions.h` table → `find_command()` resolves aliases →
  handler called with `(AppData*, WindowInfo*, args)`
- **Compact forms** (e.g. `cw5`, `jw4`): handled in `command_parser.c` COMPACT_FORMS table,
  SEPARATE from alias resolution. `parse_command_and_arg` does NOT resolve aliases —
  that happens in `find_command()`.
- **Minimize/restore**: minimized windows appear in cofi via `_NET_CLIENT_LIST`.
  `XMapRaised` in `activate_window` deiconifies naturally — no special case needed.
- **Monitor move**: `move_window_to_next_monitor()` ONLY moves. Caller (`cmd_toggle_monitor`)
  handles activation via `activate_commanded_window()`. Do NOT put `gtk_main_quit()` or
  `activate_window()` back into `move_window_to_next_monitor()`.
- **Hotkey dialog**: `show_grab_failure_dialog()` in `hotkeys.c` shows Retry/Exit on grab
  failure. Title is "Cofi — Hotkey Conflict".
