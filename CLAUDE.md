# COFI - C/GTK Window Switcher

Intent: this file is the contributor workflow and repository operating guide. It covers branching, build/test expectations, coding guidelines, and the high-level subsystem map. It is not the session log and not the place for fragile implementation gotchas.

See also:
- `checkpoint.md` for current branch/session state, recent changes, and outstanding debt
- `docs/gotchas.md` for regressions to avoid, tricky invariants, and implementation learnings
- `SPEC.md` for intended product behavior

## Development Workflow

### Branch Structure

- **`main`** — stable releases, updated via PR from develop
- **`develop`** — active development, all work merges here first
- **`fix/*`** or **`feat/*`** — short-lived branches off develop for PRs

### Rules

- **NEVER push to `develop`, `main`, or `release` without explicit user consent**
- **NEVER create merge commits — always clean rebase for clean history**
- **NEVER merge PRs that change app logic without user testing first**

### Making Changes

1. Branch from `develop` (never from `main`)
2. Make changes, build with `make clean && make`, run `make test`
3. Push branch, create PR targeting `develop`
4. Wait for user approval before merging

### Subagent PRs

When spawning agents for isolated work:
- Always instruct them to branch from `develop`
- Use `isolation: "worktree"` for parallel work
- Agent creates branch `fix/<issue-slug>`, pushes, opens PR against `develop`
- Review diff before merging — agents may branch from wrong base

### Build & Run

```bash
make clean && make      # full rebuild (required after header changes)
make test               # run all tests
./restart.sh            # clean build + restart systemd service
systemctl --user restart cofi  # restart without rebuild
journalctl --user -u cofi -f   # tail logs
```

### Tracking

- Tracking is in Linear, accessed from the `linearis` CLI
- Team: `TFD` / `IntensiCode` (`e82a4779-dde1-44e0-9772-c86ad681114f`)
- Project: `Cofi - The Comfortable Window Switcher` (`6478f7c1-ab2b-4842-ba97-74db69fed434`)
- Status flow: Backlog → Todo → In Progress → In Review → Done

### Linearis

- `linearis` is the supported way to read and update Linear from this repo
- Common commands:
  `linearis teams list`
  `linearis projects list`
  `linearis issues read TFD-82`
  `linearis issues list -l 25`
- Use issue identifiers like `TFD-82` in conversation; keep the team/project IDs above for scripts or future automation

## Coding Guidelines

1. **Write small functions** (10-15 lines, max 30)
2. **Use snake_case consistently** for functions and variables
3. **Work in small commit steps** with clear messages
4. **Write tests for essential functionality**
5. **Use logging instead of print statements**
6. **`make clean && make` after any header change** (no auto header deps yet)

## Architecture Overview

- **X11/EWMH** — direct Xlib for window management, no wmctrl
- **GTK3** — native UI with borderless always-on-top window
- **Event-driven** — PropertyNotify via GIOChannel, no polling
- **Single instance** — XGrabKey failure guards against duplicates
- **Daemon mode** — starts hidden, registers global hotkeys, waits
- **Systemd** — `make install` sets up user service with auto-restart

## Key Subsystems

- **MRU history** — focus-tracking window order, partition by type/desktop
- **Fuzzy search** — fzf-style scoring + initials + word boundary matching
- **Harpoon slots** — 36 persistent window assignments (0-9, a-z)
- **Workspace slots** — auto-numbered by screen position per workspace
- **Command mode** — vim-style `:` commands with compact syntax
- **Tiling** — half/quarter/third/grid positions, multi-monitor aware
- **Hotkeys** — configurable global X11 grabs via hotkeys.json
- **Config** — runtime-editable via `:set` or Config tab (Ctrl+T/Ctrl+E)

## File Organization

- `src/main.c` — entry point, GTK setup, key handlers, tab switching
- `src/filter.c` — search/scoring pipeline, MRU vs native ordering
- `src/display.c` — text formatting for all tabs
- `src/config.c` — config load/save/apply, build_config_entries (single source of truth)
- `src/command_mode.c` — command parsing, execution, all `:` commands
- `src/workspace_slots.c` — per-workspace slot assignment and column/row sort
- `src/hotkeys.c` — global XGrabKey registration and dispatch
- `src/harpoon.c` — persistent window slot assignments
- `src/history.c` — MRU ordering and partition_and_reorder
- `src/x11_utils.c` — X11 property extraction and window activation
- `src/x11_events.c` — event-driven window list updates
- `src/window_list.c` — EWMH window enumeration
- `src/slot_overlay.c` — numbered overlay indicators on windows
- `src/window_highlight.c` — circle ripple effect on activation
- `src/monitor_move.c` — window geometry and multi-monitor support
- `src/tiling.c` — tiling positions and calculations
- `src/log.c` — rxi/log.c logging library
