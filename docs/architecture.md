# Architecture

cofi is a single-binary C99/GTK3 X11 window switcher with daemon-mode IPC. This document describes the system shape — what the pieces are and how they wire together. For product behavior see [SPEC.md](../SPEC.md). For contributor workflow see [CLAUDE.md](../CLAUDE.md). For domain terms see [glossary.md](glossary.md).

## Process model

cofi runs as a **long-lived daemon** plus an **invocation-time delegating client**, all from the same binary.

```
┌────────────────────────────────────────────────────────────┐
│ user typed `cofi --windows` (or pressed a global hotkey)   │
└────────────────────────────────────────────────────────────┘
                          │
              ┌───────────┴───────────┐
              │ Is daemon already up? │
              │ (Unix socket bound?)  │
              └───┬───────────────┬───┘
                  │ no            │ yes
                  ▼               ▼
        ┌──────────────────┐   ┌──────────────────────────────┐
        │ Become daemon:   │   │ Send argv+opcode over socket │
        │  - bind socket   │   │ Exit immediately             │
        │  - grab hotkeys  │   └──────────────┬───────────────┘
        │  - hide window   │                  │
        │  - event loop    │ ◄────────────────┘
        └──────────────────┘   daemon receives opcode, shows
                               the requested surface
```

- **Single-instance guard.** Unix-socket listener at `$XDG_RUNTIME_DIR/cofi.sock` (fallback `/tmp/cofi.sock`). A second invocation that finds the socket bound becomes a delegator: it sends a one-byte **opcode** + argv tail, then exits. The daemon dispatches the opcode to the appropriate UI surface.
- **Daemon bootstrap.** First invocation registers global X11 hotkeys, opens a GIOChannel on the X11 connection for PropertyNotify events, hides the GTK toplevel, and waits.
- **No polling.** Window list updates come from `_NET_CLIENT_LIST` and `_NET_ACTIVE_WINDOW` PropertyNotify events, not from periodic re-enumeration. MRU history is maintained from focus changes.
- **systemd integration.** `mise run install` installs a user service (`scripts/cofi.service`) that auto-restarts on crash.

## Subsystem map

```
                    ┌───────────────────────────────┐
                    │           main.c              │
                    │  argv → daemon or delegate    │
                    └───────────────┬───────────────┘
                                    │
        ┌───────────────────────────┼───────────────────────────┐
        ▼                           ▼                           ▼
┌─────────────────┐   ┌─────────────────────────┐   ┌─────────────────────┐
│ daemon_socket   │   │  GTK UI                 │   │ X11 backend         │
│  - bind/listen  │   │  - Windows core tab     │   │  - x11_utils        │
│  - opcode       │   │  - provider tabs        │   │  - x11_events       │
│    dispatch     │   │  - command mode (`:`)   │   │  - window_list      │
│                 │   │  - modal prefixes       │   │  - hotkeys          │
│                 │   │  - slot overlays        │   │    (XGrabKey)       │
│                 │   │  - key dispatch         │   │  - monitor_move     │
└─────────────────┘   └─────────┬───────────────┘   └─────────────────────┘
                                │
                                ▼
                ┌────────────────────────────────────┐
                │ Provider registry + data modules   │
                │  - cofi_tab_provider registry      │
                │  - workspaces / harpoon / names    │
                │  - config / hotkeys / rules / apps │
                │  - calc / sinks / run / proc       │
                │  - sessions / profiles             │
                │  - history + filter + display      │
                │  - config/state persistence        │
                └────────────────────────────────────┘
```

## Layer responsibilities

### Entry & dispatch

- **`src/main.c`** — argv parse, decide daemon vs delegate, GTK setup, daemon bootstrap, handoff to UI surface.
- **`src/cli_args.cpp`** — popl-based CLI option parsing (`--windows`, `--workspaces`, `--harpoon`, `--command`, `--run`, `--applications`, `--assign-slots`, etc.).
- **`src/command_parse_defs.c`** — parse metadata for `:` commands: primary names, aliases, compact suffixes, and explicit command owner (`core` or provider id). This table is still central by design today; TFD-675 tracks moving provider-owned command metadata into owner modules over time.
- **`src/command_parser.c`** — compact-syntax splitter (`tw3`, `jw1`) and alias resolution using the parse metadata.
- **`src/command_availability.c`** — owner-aware availability gate for command candidates, help, and dispatch.
- **`src/command_mode.c`** + `src/command_handlers*.c` — execution of `:` commands.

### IPC

- **`src/daemon_socket.c`** — protocol primitives (path resolution, bind, connect, send, accept).
- **`src/daemon_socket_runtime.c`** — GIOChannel integration, opcode-to-UI dispatch (`COFI_OPCODE_WINDOWS`, `_WORKSPACES`, `_HARPOON`, `_NAMES`, `_COMMAND`, `_RUN`, `_APPLICATIONS`).

### X11

- **`src/x11_utils.c`** — EWMH property extraction (`_NET_WM_NAME`, `_NET_WM_PID`, etc.), window activation via `_NET_ACTIVE_WINDOW` ClientMessage.
- **`src/x11_events.c`** — event-driven window list updates from PropertyNotify on root.
- **`src/window_list.c`** — `_NET_CLIENT_LIST` enumeration + filtering (skip-taskbar, types).
- **`src/hotkeys.c`** — `XGrabKey` registration and dispatch from KeyPress events.
- **`src/monitor_move.c`** — XRandR geometry + work-area calculation for tiling and multi-monitor.

### UI

- **`src/window_lifecycle.c`** — show/hide of the cofi toplevel. Recomputes Pango font metrics + window size + monitor placement on every show (handles XSettings/DPI changes mid-session).
- **`src/cofi_tab_provider.c`** — provider registry. Providers register tabs, command aliases, modal prefixes, slots, tick callbacks, dynamic tab handles, and enablement metadata.
- **`src/display.c`** — top-level display assembly. Windows remains core-special; provider tabs render through `CofiTabProvider` row callbacks.
- **`src/display_pipeline.c`** — assembly of the display strings from filter results.
- **`src/tab_header.c`** + **`src/tab_switching.c`** — tab header formatting, overflow, visibility, and cycling. Header/cycling enumerate Windows plus registered provider tabs, not a fixed `TAB_*` loop.
- **`src/key_handler.c`** + `src/key_handler_*.c` — core key dispatch and mode precedence. Provider-specific key handling lives on provider `handle_key` callbacks.
- **`src/slot_overlay.c`** — transient `[N]` indicators drawn on each window.
- **`src/window_highlight.c`** — circle ripple effect on activation.

### Providers and plugin architecture

- **Provider tabs** — list-with-action surfaces registered through `CofiTabProvider`. Current provider tabs are Workspaces, Harpoon, Names, Config, Hotkeys, Rules, Apps, Calc, Sinks, Run, Proc, Sessions, and Profiles.
- **Dynamic tab handles** — new provider tabs can request `COFI_PROVIDER_DYNAMIC_TAB` and receive a runtime tab handle. Existing providers still use legacy `TAB_*` handles for compatibility.
- **Enablement** — providers stay registered but can be disabled through config. Registry lookups for tab, command, and prefix surfaces fail closed for disabled providers. Required providers, currently Config, cannot be disabled.
- **Commands** — command parse metadata still lives centrally, but every command now has an explicit owner. Provider-owned commands are hidden when that provider is disabled.
- **Plan** — [docs/decisions/0004-plugin-architecture-plan.md](decisions/0004-plugin-architecture-plan.md) is the current TFD-675 roadmap for reducing remaining central tables and hardcoded surfaces.

### Data & filtering

- **`src/history.c`** — MRU list; `partition_and_reorder` splits by type/desktop.
- **`src/filter.c`** — fzf-style scoring, search/MRU/native ordering modes.
- **`src/fzf_algo.c`** — the scoring algorithm.
- **`src/match.c`** — match-stage classification (exact / prefix / initials / fuzzy).
- **`src/harpoon.c`** — 36 persistent slot assignments (`~/.config/cofi/harpoon.json`).
- **`src/workspace_slots.c`** — per-workspace auto-numbered slots (column-major).
- **`src/named_window.c`** + `src/window_matcher.c` — user-assigned names + matching rules.
- **`src/rules.c`** + `src/rules_config.c` — rule-based window classification.

### Config

- **`src/config.c`** — load/save/apply config; `build_config_entries` is the single source of truth for editable keys (consumed by `:set` and the Config tab).
- **`src/hotkey_config.c`** — `hotkeys.json` parsing and grab registration.

### Modes

- **`src/run_mode.c`** — `!` prefix; session-only history; detached shell launch.
- **`src/apps.c`** — desktop-app loader, Apps-local matching/ranking, detached launch.
- **`src/path_binaries.c`** — async `$PATH` scan with `GFileMonitor` watchers.
- **`src/system_actions.c`** — logind D-Bus calls (Lock, Suspend, Hibernate, Logout, Reboot, Shutdown) with shell fallback.
- **`src/detach_launch.c`** — shared detached-launch helpers (`systemd-run` primary, `fork+setsid` fallback).
- **`src/tiling.c`** — half/quarter/third/grid geometry calculation.

### Infrastructure

- **`src/log.c`** — rxi/log.c bundled logging library.
- **`src/utils.c`** — string and path helpers.

## Cross-cutting invariants

These are the rules that don't live in any one file but must hold across the system. The full list of regressions to avoid is in [docs/gotchas.md](gotchas.md).

- **MRU before display.** Filter/scoring runs against MRU-ordered candidates, not the native EWMH order. Reordering for display happens once, after scoring.
- **Display order = search order.** Never reorder the match-target string vs the display columns — the search string is what scoring sees.
- **Cache invalidation on show.** Pango/monitor/DPI state is sampled per show, not at startup, to survive `xrandr` and XSettings (`Xft/DPI`) changes mid-session.
- **Single source of truth for config keys.** `build_config_entries` in `src/config.c` — `:set` and the Config tab both read from it. Adding a key in one place without the other is a bug.
- **Detached launch.** Anything cofi launches (run mode, apps tab, `:run`) must outlive cofi itself. `systemd-run --scope --user` is the primary path; `fork+setsid` is the fallback.

## Build & test

- **Build:** `mise run build` wraps `make` (incremental, header deps via `-MMD -MP` in `.d` files). After header changes, `mise run rebuild` because the dep tracking is partial.
- **Test:** `mise run test` wraps `make test`, which builds and runs 70 standalone test binaries from `test/test_*.c`. Each test links specific `src/*.o` files plus stubs for cross-cutting deps it doesn't exercise.
- **CI:** `.github/workflows/build.yml` runs `make` + `make test` on every push.
- **Pre-push hook:** `scripts/hooks/pre-push` runs `make test` locally so a Git hook does not require mise; install via `bash scripts/install-hooks.sh`.
