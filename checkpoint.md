# Cofi Development Checkpoint — 2026-04-07

Intent: this file is the current-state checkpoint for active development. It tracks branch state, recent completed work, known issues, and operational context needed to resume quickly. It is not the canonical workflow guide and not the long-term home for cross-cutting gotchas.

See also:
- `CLAUDE.md` for contributor workflow, branching rules, and repo conventions
- `docs/gotchas.md` for fragile behaviors, regressions to avoid, and implementation learnings
- `SPEC.md` for intended product behavior

---

## Repo & Infra

- **Repo**: https://github.com/DanielLukic/cofi
- **Owner**: DanielLukic (transferred from IntensiCode)
- **Default branch**: `develop`
- **Tracking**: Linear via `linearis`
- **Team**: `TFD` / `IntensiCode` (`e82a4779-dde1-44e0-9772-c86ad681114f`)
- **Project**: `Cofi - The Comfortable Window Switcher` (`6478f7c1-ab2b-4842-ba97-74db69fed434`)
- **Systemd service**: `~/.config/systemd/user/cofi.service`
- **Restart**: `./restart.sh` — does `make clean && make`, restarts service, shows last log lines
- **Logs**: `journalctl --user -u cofi -f`
- **Linear CLI**: `linearis`

---

## Critical Rules (in CLAUDE.md)

1. **NEVER push to develop/main/release without explicit user consent**
2. **NEVER create merge commits — always clean rebase**
3. **Direct session work goes straight to develop (no PR needed)**
4. **Subagent work uses `.worktrees/` inside project root**
   - `git worktree add .worktrees/<slug> -b <branch>`
   - `.worktrees/` is gitignored
5. **App logic PRs need user testing before merge; trivial (docs/dead code) can merge immediately**
6. **After any header (.h) change: `make clean && make` required**
7. **TDD for refactoring ≠ TDD for features** (see CLAUDE.md rule 7):
   - Refactoring: write *passing* behavioral tests against existing code first, then refactor
   - Feature: write *failing* tests first, then implement

---

## Current Branch State

Branch: `develop`. All work is local — nothing pushed to origin since `0aa26f8`.

### Recent commits (newest first):

```
e52c5bd Fix workspace slot assignment cap after occlusion filtering
9d5b471 chore: update checkpoint.md
ac43cc0 Split main.c into focused lifecycle, tabs, key, and setup modules
b11a648 Add behavioral regression tests for split command handlers
ca7d590 docs: clarify TDD rule for refactoring vs feature work
56154e5 Split command handlers into domain modules
d22206b Test init_app_data hotkey grab-state initialization path
909813f Extract command API header and add chain policy tests
```

---

## Recently Completed Work

### Major refactoring wave (TFD-255 through TFD-268)

All 8 refactoring tickets completed. Before/after summary:

| File | Before | After | Ticket |
|------|--------|-------|--------|
| `src/main.c` | 1531L | 7L | TFD-255 |
| `src/command_mode.c` | 1330L | 381L | TFD-259 |
| `src/overlay_manager.c` | 866L | 147L | TFD-257 |
| `src/display.c` | 771L | 697L | TFD-256 |
| `src/command_handlers.c` (was in command_mode.c) | — | 164L + 4 domain files | TFD-268 |

New files introduced:
- `src/app_setup.c` (289L) — GTK setup + `run_cofi()` startup
- `src/key_handler.c` (447L) — all keyboard input handling
- `src/tab_switching.c` (210L) — tab switching + per-tab filter functions
- `src/window_lifecycle.c` (276L) — show/hide/destroy/focus lifecycle
- `src/hotkey_dispatch.c` (93L) — hotkey show-mode dispatch + delayed command mode
- `src/command_handlers_window.c` (301L) — per-window cmd_* handlers
- `src/command_handlers_workspace.c` (177L) — workspace cmd_* handlers
- `src/command_handlers_tiling.c` (152L) — tiling + mouse cmd_* handlers
- `src/command_handlers_ui.c` (195L) — UI/config cmd_* handlers
- `src/command_api.h` (23L) — shared non-UI command declarations
- `src/display_pipeline.c` (57L) — shared tab formatter pipeline
- `src/hotkey_grab_state.c` (75L) — `populate_hotkey_grab_state()` builder
- `src/overlay_dispatch.c` (132L) — overlay create/dispatch routing
- `src/overlay_hotkey_add.c`, `src/overlay_hotkey_edit.c`, `src/overlay_harpoon.c`, `src/overlay_name.c`, `src/overlay_config.c`, `src/overlay_workspace.c` — one file per overlay type

### Bug fixes (same session)

- **KP_Enter capture** (0506cb8): `should_capture_hotkey_event()` and `handle_hotkey_add_key_press()` were blocking modifier+KP_Enter unconditionally. Fixed to only block bare KP_Enter.
- **KP_Decimal not firing** (0506cb8): `XKeysymToKeycode` only finds column-0 keycodes; physical numpad dot (kc 91) has KP_Decimal at column 1. Fixed by scanning all keycodes/columns in grab, ungrab, and dispatch.
- **Fixed window sizing** (TFD-100): One-shot `size-allocate` init sets `fixed_cols`/`fixed_rows` in AppData after first map. Scrollbar race on first render fixed.

### Hotkey grab state encapsulation (TFD-258)

- `GrabbedHotkey` + `HotkeyGrabState` structs in `hotkeys.h`
- `app->hotkey_grab_state` replaces file-scope globals in `hotkeys.c`
- `grab_error_occurred` stays as file-scope static (reset per grab attempt) — nested function approach was rejected: caused executable stack (`RWE`) via GCC trampoline

### Follow-up tickets created (TFD-265/266)

- TFD-265: command API header decoupling + alias drift guard + chain tests → **Done**
- TFD-266: `init_app_data` → `hotkey_grab_state` end-to-end test → **Done**

---

## Test Suite

**~951 total assertions** across all test files. `make test` runs all.

| Test file | Assertions |
|-----------|------------|
| test_command_aliases | 115 |
| test_fzf_algo | 46 |
| test_command_dispatch | 86 |
| test_named_window | 55 |
| test_command_parsing | 47 |
| test_command_set | 46 |
| test_hotkey_config | 46 |
| test_parse_shortcut | 89 (run_tests) |
| test_wildcard_match | 49 |
| test_match_scoring | 36 |
| test_config_roundtrip | 32 |
| test_scrollbar | 43 |
| test_rules | 43 |
| test_command_dispatch | 86 |
| test_dynamic_display_fixed | 9 |
| test_display_pipeline | 12 |
| test_overlay_dispatch | 15 |
| test_hotkey_grab_state | 9 |
| test_command_parser_execution | 8 |
| test_command_handlers_split | 8 |
| test_command_handlers_behavior | 12 |
| test_main_split_regression | 8 |

All passing as of last run.

**NOT in run_tests.sh** (need to verify): none known outstanding.

---

## Open Linear Tickets

| ID | Title | Status |
|----|-------|--------|
| TFD-269 | Investigate and fix window-ready timing: replace command_mode_timer with post-map action queue | Backlog |
| TFD-270 | Split key_handler.c into focused sub-modules | Backlog |
| TFD-285 | Fix: workspace slot cap applied before occlusion silently drops windows | Done |
| TFD-267 | Investigate: --screenshot=<delay_ms> CLI option for visual regression testing | Backlog |
| TFD-82 | Hotkey capture mode | Backlog |

### Recently Done (this session)
TFD-100, TFD-255, TFD-256, TFD-257, TFD-258, TFD-259, TFD-265, TFD-266, TFD-268, TFD-285

---

## Key File Map (post-refactor)

| File | Purpose |
|------|---------|
| `src/main.c` | 7 lines — calls `run_cofi()` |
| `src/app_setup.c` | GTK window construction, `setup_application()`, `run_cofi()` startup |
| `src/key_handler.c` | All keyboard input: `on_key_press`, navigation, per-tab handlers, harpoon keys |
| `src/tab_switching.c` | `switch_to_tab()`, `filter_*()` per tab |
| `src/window_lifecycle.c` | `show_window()`, `hide_window()`, focus/close events |
| `src/hotkey_dispatch.c` | `dispatch_hotkey_mode()`, `delayed_command_mode()`, `command_mode_timer` |
| `src/command_mode.c` | Command mode UI: entry, cursor, history, help rendering |
| `src/command_handlers.c` | Execute pipeline: `execute_command_with_window()`, keep-open policy |
| `src/command_handlers_window.c` | Window cmd_* implementations |
| `src/command_handlers_workspace.c` | Workspace cmd_* implementations |
| `src/command_handlers_tiling.c` | Tiling + mouse cmd_* implementations |
| `src/command_handlers_ui.c` | UI/config/hotkey cmd_* implementations |
| `src/command_parser.c` | All parsing: compact forms, alias resolution, chain tokenization |
| `src/command_definitions.h` | Command table: names, aliases, handler pointers, help text |
| `src/display.c` | Tab formatters using shared `display_pipeline` |
| `src/display_pipeline.c` | Shared pagination/render pipeline for all tabs |
| `src/overlay_manager.c` | Overlay lifecycle: show/hide/dispatch (~147L) |
| `src/overlay_*.c` | One file per overlay type |
| `src/hotkeys.c` | XGrabKey registration + dispatch using `app->hotkey_grab_state` |
| `src/hotkey_grab_state.c` | `populate_hotkey_grab_state()` builder |
| `src/dynamic_display.c` | Fixed window sizing, font metrics, `init_fixed_window_size()` |
| `src/app_data.h` | AppData struct — single source of truth for all app state |
| `src/app_init.c` | `init_app_data()` — zero-initializes all AppData fields |
| `src/config.c` | Config load/save/apply, `build_config_entries()` |
| `src/filter.c` | Search/scoring pipeline, MRU vs native ordering |
| `src/history.c` | MRU ordering, `partition_and_reorder()` |
| `src/x11_utils.c` | X11 property extraction, `activate_window()`, `minimize_window()` |

---

## Known Issues / Tech Debt

0. **Workspace slot cap bug — fixed** (TFD-285, e52c5bd): `MAX_WORKSPACE_SLOTS=9` was used as both collection cap and slot cap. Occlusion filtering happens after collection, so a sticky window consuming the 9th candidate slot silently dropped real visible windows. Fixed: collect all qualifying windows, apply occlusion, then cap to MAX_WORKSPACE_SLOTS at assignment only.

1. **`command_mode_timer` cross-module coupling** (TFD-269) — `window_lifecycle.c` directly references `command_mode_timer` global from `hotkey_dispatch.c`. Also: three inconsistent window-ready paths (path 1 uses 50ms timer, paths 2+3 don't). Critical investigation needed before fix.

2. **`key_handler.c` at 447 lines** (TFD-270) — largest file post-refactor. Should split into harpoon keys + tab keys + core navigation.

3. **Fixed window sizing init flag race** (TFD-100, in gotchas.md) — `fixed_window_size_initializing` cleared via `g_idle_add` may race with resize-triggered `size-allocate`. Low impact.

4. **Keep-open policy duplication** — `COMMAND_DEFINITIONS[*].keeps_open_on_hotkey_auto` and hardcoded lists in `command_handlers.c` can drift silently. No ticket yet.

5. **`cmd_mouse` in tiling handlers** — slightly awkward domain fit; should eventually move to window handlers or rename file to `command_handlers_placement.c`.

6. **`COMMAND_POLICY_ONLY` compile flag** in `command_handlers.c` — tactical test isolation seam; watch for it spreading.

7. **Handler behavioral test coverage is thin** — `test_command_handlers_behavior.c` covers one representative per domain. Deeper coverage would require more stubs or integration tests.

8. **No visual/UI regression tests** — TFD-267 investigates `--screenshot=<delay_ms>` as a solution.

---

## Architecture Notes

- **Command dispatch**: `command_definitions.h` table → `command_parser.c` resolves aliases → `command_handlers.c` `execute_command_with_window()` → domain handler files
- **Hotkey flow**: X11 `PropertyNotify` → `handle_hotkey_event()` → `g_idle_add(hotkey_dispatch_idle)` → `dispatch_hotkey_mode()` or `prefill_command_mode()`
- **Display pipeline**: all 6 tabs use `render_display_pipeline()` in `display_pipeline.c` — tab formatters only provide item count + `render_item()` callback
- **Overlay system**: `show_overlay(type)` → `overlay_dispatch.c` creates content → per-type file handles key press
- **Fixed window sizing**: one-shot `size-allocate` on textview → `init_fixed_window_size()` → `app->fixed_cols`/`fixed_rows` → all formatters use these values
- **Hotkey grab**: `app->hotkey_grab_state` (not globals) — `populate_hotkey_grab_state()` builds from `HotkeyConfig`, `find_keycodes_for_sym()` scans all columns (fixes KP_Decimal/column-1 keysyms)

---

## Multi-Agent Workflow

- **MainAgent** (this agent): chief engineer — plans, reviews, merges
- **FastIce** (GPT-5.3-codex or similar frontier): implements tickets
- **assistant** (devstral-medium or similar): code review only — no implementation
- Pattern: FastIce implements in `.worktrees/<slug>` → MainAgent verifies → assistant reviews → MainAgent merges to develop
- Key lesson: devstral-medium struggles with files >500L and runs out of context on large refactors. Use for review only or small targeted tasks.

---

## Multi-Agent Workflow Notes

- **assistant** is now running GPT-5.4 (not devstral) — accurate and fast for analysis/review
- **FastHawk** is GPT-5.3-codex — solid implementer, handles large refactors well
- **Bug hunt pattern**: MainAgent gathers data → sends to both agents simultaneously → assistant diagnoses → FastHawk implements → assistant reviews → MainAgent merges
- TFD-285 was found and fixed entirely within one session using this pattern (~30min from bug report to live verification)

---

## How to Resume

1. `cd /home/dl/Projects/cofi`
2. `git log --oneline -5` — verify you're on develop at the right commit
3. `make test` — verify all tests pass
4. `systemctl --user status cofi` — verify service is running
5. `linearis issues list -l 10` — check current ticket state
6. Read this file + `docs/gotchas.md` for context
7. Next up: **TFD-269** (window-ready timing investigation) or **TFD-270** (key_handler split)
