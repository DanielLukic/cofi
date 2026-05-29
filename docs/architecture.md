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
                │  - workspaces / harpoon / matching │
                │  - config / hotkeys / rules / apps │
                │  - calc / sinks / run / proc       │
                │  - projects / profiles             │
                │  - history + filter + display      │
                │  - config/state persistence        │
                └────────────────────────────────────┘
```

## Layer responsibilities

### Entry & dispatch

- **`src/main.c`** — argv parse, decide daemon vs delegate, GTK setup, daemon bootstrap, handoff to UI surface.
- **`src/cli_args.cpp`** — popl-based CLI option parsing (`--windows`, `--workspaces`, `--harpoon`, `--matching`, `--names`, `--command`, `--run`, `--applications`, `--assign-slots`, etc.).
- **`src/command_registry.c`** — command storage and lookup registry: primary names, aliases, compact suffixes, handlers, help text, activation policy, keep-open policy, and explicit owner.
- **`src/core_commands.c`** — built-in core command registration. Provider-owned commands register from their provider modules.
- **`src/builtin_plugins.c`** — compiled-in plugin/provider registration list used during startup.
- **`src/command_parser.c`** — compact-syntax splitter (`tw3`, `jw1`) and alias resolution using the command registry.
- **`src/command_availability.c`** — owner-aware availability gate for command candidates, help, and dispatch.
- **`src/command_mode.c`**, **`src/command_handlers*.c`**, and provider-owned `CommandSpec` handlers — execution of `:` commands.

### IPC

- **`src/daemon_socket.c`** — protocol primitives (path resolution, bind, connect, send, accept).
- **`src/daemon_socket_runtime.c`** — GIOChannel integration, opcode-to-UI dispatch (`COFI_OPCODE_WINDOWS`, `_WORKSPACES`, `_HARPOON`, `_MATCHING`, `_NAMES`, `_COMMAND`, `_RUN`, `_APPLICATIONS`).

### X11

- **`src/x11_utils.c`** — EWMH property extraction (`_NET_WM_NAME`, `_NET_WM_PID`, etc.), window activation via `_NET_ACTIVE_WINDOW` ClientMessage.
- **`src/x11_events.c`** — event-driven window list updates from PropertyNotify on root.
- **`src/window_list.c`** — `_NET_CLIENT_LIST` enumeration + filtering (skip-taskbar, types).
- **`src/hotkeys.c`** — `XGrabKey` registration and dispatch from KeyPress events.
- **`src/monitor_move.c`** — XRandR geometry + work-area calculation for tiling and multi-monitor.

### UI

- **`src/window_lifecycle.c`** — show/hide of the cofi toplevel. Recomputes Pango font metrics + window size + monitor placement on every show (handles XSettings/DPI changes mid-session).
- **`src/cofi_tab_provider.c`** — provider registry. Providers register tabs, modal prefixes, delegate opcodes, hotkey mode claims, slots, tick callbacks, dynamic tab handles, and enablement metadata.
- **`src/display.c`** — top-level display assembly. Windows remains core-special; provider tabs render through `CofiTabProvider` row callbacks.
- **`src/display_pipeline.c`** — assembly of the display strings from filter results.
- **`src/tab_header.c`** + **`src/tab_switching.c`** — tab header formatting, overflow, visibility, and cycling. Header/cycling enumerate Windows plus registered provider tabs, not a fixed `TAB_*` loop.
- **`src/key_handler.c`** + `src/key_handler_*.c` — core key dispatch and mode precedence. Provider-specific key handling lives on provider `handle_key` callbacks.
- **`src/slot_overlay.c`** — transient `[N]` indicators drawn on each window.
- **`src/window_highlight.c`** — circle ripple effect on activation.

### Providers and plugin architecture

- **Provider tabs** — list-with-action surfaces registered through `CofiTabProvider`. Current provider tabs are Sessions, Workspaces, Harpoon, Names, Config, Hotkeys, Rules, Apps, Calc, Sinks, Run, Proc, Projects, and Profiles.
- **Dynamic tab handles** — provider tabs request `COFI_PROVIDER_DYNAMIC_TAB` and receive a runtime tab handle. `TabMode` is now core-only (`TAB_WINDOWS` plus the `TAB_COUNT` sentinel); provider tabs are enumerated through the registry.
- **Enablement** — providers stay registered but can be disabled through config. Registry lookups for tab, command, and prefix surfaces fail closed for disabled providers. Required providers, currently Config, cannot be disabled.
- **Commands** — commands register `CommandSpec` entries with `command_registry`. Provider command specs live in their provider modules; core commands live in the built-in core registration list. Provider-owned commands are hidden when that provider is disabled.
- **Delegates** — provider-backed daemon opcodes and hotkey modes resolve through provider metadata, keeping compatibility constants out of provider-specific dispatch code.
- **Built-ins** — `builtin_plugins.c` is the only startup module that should know the compiled-in plugin/provider list.
- **Plan** — [docs/decisions/0004-plugin-architecture-plan.md](decisions/0004-plugin-architecture-plan.md) is the current TFD-675 roadmap for reducing remaining central tables and hardcoded surfaces.

### Data & filtering

- **`src/history.c`** — MRU list; `partition_and_reorder` splits by type/desktop.
- **`src/ui/window_filter.c`** — Windows-tab filtering, search/MRU/native ordering modes, workspace bias, and display-title search strings.
- **`src/fzf_algo.c`** — the scoring algorithm.
- **`src/match.c`** — match-stage classification (exact / prefix / initials / fuzzy).
- **`src/harpoon.c`** — 36 persistent slot assignments (`~/.config/cofi/harpoon.json`).
- **`src/workspace_slots.c`** — per-workspace auto-numbered slots (column-major).
- **`src/match_entry.c`** + `src/window_matcher.c` — pure matching identities and match-entry pattern/anchor evaluation.
- **`src/names_store.c`** + `src/names_provider.c` — user-assigned custom names keyed one-to-one by match id.
- **`src/rules.c`** + `src/rules_config.c` — rule-based window classification.
- **`src/sessions.c`** — live cancellable `rg` search over Claude/Codex JSONL session files; groups raw matches into session rows and applies `terms | refine` filtering without a persistent index.

### Config

- **`src/config.c`** — load/save/apply config. Built-in keys and provider-registered `CofiConfigSpec` entries share one registry path consumed by `:set`, save/load, and the Config tab.
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
- **`src/cofi_json_io.c`** — tolerant JSON I/O wrapper over `json-glib`. Convergence point for persistence stores; codifies the unknown-field / missing-key / wrong-type / corrupt-file policy in one place. See [ADR-0009](adr/0009-tolerant-json-io-via-json-glib.md). Migration to it is staged per-store; some legacy stores still use hand-rolled `fprintf`/`sscanf`.

## Cross-cutting invariants

These are the rules that don't live in any one file but must hold across the system. The full list of regressions to avoid is in [docs/gotchas.md](gotchas.md).

- **Inverted-list render.** Index 0 is the visible *bottom* of the list; `render_display_pipeline` iterates `end−1 → start`. Any code that maps filter rank to display position must account for this reversal (see ADR-0007).
- **MRU before display.** Filter/scoring runs against MRU-ordered candidates, not the native EWMH order. Reordering for display happens once, after scoring.
- **Display order = search order.** Never reorder the match-target string vs the display columns — the search string is what scoring sees.
- **Cache invalidation on show.** Pango/monitor/DPI state is sampled per show, not at startup, to survive `xrandr` and XSettings (`Xft/DPI`) changes mid-session.
- **Single source of truth for config keys.** Config descriptors in `src/config.c` drive save/load, `:set`, and Config tab rows. Provider-owned keys use a `provider_id.key_name` namespace and must be registered during provider bootstrap.
- **Detached launch.** Anything cofi launches (run mode, apps tab, `:run`) must outlive cofi itself. `systemd-run --scope --user` is the primary path; `fork+setsid` is the fallback.
- **Rules-as-policy boundary.** Auto-apply behaviors on window-appear/title-change (geom restore, sticky, always-above, workspace pin) attach via the rules engine, not bespoke triggers. A single execution path keeps the fire-once + circuit-breaker + idempotency invariants in one place — fixes there pay off everywhere. Geom auto-restore is the canonical example: layout payload in `layouts.json` keyed by match_id; trigger via `pattern → rl` rule.
- **Tolerant persistence I/O.** All JSON reads through `cofi_json_io` (migration in progress, see [ADR-0009](adr/0009-tolerant-json-io-via-json-glib.md)) treat unknown fields as forward-compat, missing/wrong-type as `present == FALSE` + fallback, corrupt files as `log_error` + empty-load. Stores never abort on bad input. Atomic save via tmp+rename in the same directory.
- **Selection-by-identity on non-query refresh.** Any list refresh that does not change the query/filter membership — data updates, flag toggles, tick callbacks, row mutations — must preserve the selected row by calling `preserve_selection()` before the refresh and `restore_selection()` after; only a genuine `on_query_changed` path may move selection to the best/top match. When the selected row is gone after refresh, the fallback is the nearest surviving row (same index if in range, else previous), not the top. See `src/core/selection/CONTEXT.md` criteria 6–8 and TFD-835 for known non-compliant paths.
- **Single owner per match id.** Each persisted `match_id` has exactly one owning subsystem record: Harpoon slot, Names record, geom layout, rule, or another explicit owner. `MatchEntry` itself is pure identity and not an owner label store. The Names subsystem owns custom names as a one-to-one `match_id -> custom_name` store. Owners delete their own match entry directly when deleting their record; startup matching GC is only a crash/legacy-data seatbelt over owner roots.

## Build & test

- **Build:** `mise run build` wraps `make` (incremental, header deps via `-MMD -MP` in `.d` files). After header changes, `mise run rebuild` because the dep tracking is partial.
- **Test:** `mise run test` wraps `make test`, which builds and runs 70 standalone test binaries from `test/test_*.c`. Each test links specific `src/*.o` files plus stubs for cross-cutting deps it doesn't exercise.
- **CI:** `.github/workflows/build.yml` runs `make` + `make test` on every push.
- **Pre-push hook:** `scripts/hooks/pre-push` checks out each pushed ref tip in a temporary detached worktree and runs `make test` there, so the tested tree matches the ref being pushed without requiring mise; install via `bash scripts/install-hooks.sh`.
