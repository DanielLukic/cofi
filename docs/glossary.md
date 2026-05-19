# Glossary

Domain terms used throughout cofi's code and docs. Defined once here so new contributors don't have to reverse-engineer them.

## Window & workspace

- **Window** — a top-level X11 client window managed by the WM (matches EWMH `_NET_CLIENT_LIST`). cofi excludes windows with `_NET_WM_STATE_SKIP_TASKBAR` set.
- **Workspace** — an X11 virtual desktop (EWMH `_NET_CURRENT_DESKTOP`). cofi uses "workspace" and "desktop" interchangeably; the code prefers `desktop` for the integer index and `workspace` for the user-facing name.
- **Active window / focused window** — the window with input focus (`_NET_ACTIVE_WINDOW`).
- **Monitor** — a physical output reported by XRandR. cofi is multi-monitor aware for tiling and slot assignment.

## MRU & history

- **MRU** — Most Recently Used. cofi tracks focus order via PropertyNotify on `_NET_ACTIVE_WINDOW` rather than polling. The MRU list is the source of truth for default ordering in the Windows tab.
- **partition_and_reorder** — internal function (`src/history.c`) that splits the window list by type/desktop and re-emits in MRU order. Used to keep the current desktop's windows ahead of others when configured.

## Harpoon

- **Harpoon slot** — one of 36 persistent assignments (digits `0–9` plus letters `a–z`) mapping a key to a specific window. Inspired by the Neovim plugin of the same name. Stored in `~/.config/cofi/harpoon.json` and survives restarts.
- **Harpoon tab** — the UI surface for editing/jumping harpoon assignments. Shortcut: `Ctrl+H`.
- **Provider slot** — a slot owned by a provider tab rather than the Windows tab. Sinks, Projects, and Profiles use provider-specific payloads so the same key can mean different things per tab.

## Workspace slots

- **Workspace slot** — an auto-assigned 1–9 index for windows on the current workspace, sorted by screen position (column-major). Distinct from harpoon: slots are computed live, not persisted.
- **Slot overlay** — a transient `[N]` indicator drawn on each window showing its current slot. Toggled by Alt and during the per-workspace digit-slot mode.
- **digit slot mode** — a config setting controlling what Alt+digit does:
  - `default` — Alt+digit jumps to harpoon slot
  - `workspaces` — Alt+digit jumps to workspace N
  - `per-workspace` — Alt+digit jumps to workspace slot N within the current workspace

## Modes & tabs

- **Tab** — a view in the cofi window. Windows is core-special; Sessions, Workspaces, Harpoon, Names, Config, Hotkeys, Rules, Apps, Calc, Sinks, Run, Proc, Projects, and Profiles are provider tabs.
- **Provider** — a compiled-in list/action surface registered through `CofiTabProvider`. Providers own row formatting, filtering hooks, Enter behavior, tab-specific keys, command aliases, optional prefixes, slots, and tick callbacks.
- **Plugin** — broader architecture term for a compiled-in capability module. A plugin may expose a provider tab, commands, prefixes, slots, config rows, or later rule predicates/actions. Today most plugin work is represented by providers.
- **Dynamic tab handle** — a runtime tab id assigned when a provider registers with `COFI_PROVIDER_DYNAMIC_TAB`. Provider tabs no longer have static `TAB_*` enum values; code should resolve them through provider helpers or registry lookups.
- **Provider enablement** — config-driven enabled/disabled state for providers. Disabled providers disappear from tabs, command candidates/help, command dispatch, prefixes, and slots; required providers cannot be disabled.
- **Command mode** — vim-style `:` prefix entering compact commands (see `:help`). Implemented in `src/command_mode.c`; core command metadata lives in `src/core_commands.c`, provider-owned commands live in provider modules, and `src/command_registry.c` indexes both.
- **Modal provider mode** — a provider-owned temporary mode entered by a prefix such as `!` (Run) or `=` (Calc). Core owns the modal lifecycle; the provider owns rows and actions.
- **Run mode** — `!` prefix for launching shell commands with session-only history. Backed by `src/run_mode.c` and surfaced through the Run provider.
- **Sessions tab** — live search over Claude/Codex session JSONL files. It intentionally does not build a persistent index; every new left-side query starts a fresh cancellable `rg` process. Query shape is `terms | refine`: left side searches the corpus, right side fuzzily refines grouped session rows.
- **Auto-execute marker** — an entered query starting with `!` that triggers immediate launch on Enter without confirmation.
- **raw_idx / filtered_idx** — provider callbacks receive raw provider row indices for row data, while core selection state stores filtered/visible indices. `cofi_filtered_to_raw()` bridges the two.

## Daemon

- **Daemon** — the long-lived cofi process. Starts hidden, registers global X11 hotkeys, listens on a Unix socket. Single-instance guard at `$XDG_RUNTIME_DIR/cofi.sock` (fallback `/tmp/cofi.sock`).
- **Delegation** — when a second `cofi` invocation finds the socket already bound, it sends its argv+opcode to the running daemon instead of starting a second instance. Opcodes are defined in `src/daemon_socket.h`.
- **Opcode** — single-byte tag (`COFI_OPCODE_*`) identifying what surface the delegating invocation wants opened (Windows, Workspaces, Harpoon, command mode, run mode, etc.).

## UI primitives

- **Slot overlay** — see above under Workspace slots.
- **Window highlight** — circle ripple effect drawn on a window when cofi activates it. Implemented in `src/window_highlight.c`.
- **Fixed window size** — cofi's main window has a precomputed width × height derived from Pango font metrics + monitor work area. Recomputed on each `show_window` to handle XSettings/DPI changes (see `src/window_lifecycle.c`).

## Tiling

- **Tile** — place a window at a predefined geometry (half/quarter/third/grid). Driven by the `:tw` command and the `tw` digit/letter argument; logic in `src/tiling.c`.
- **Work area** — the monitor geometry minus reserved struts (panels, docks). cofi tiles within the work area, not the full screen.

## Config

- **Config tab** — `Ctrl+E` opens the runtime-editable config UI; written changes persist to `~/.config/cofi/options.json`.
- **`:set` command** — change a single config key from command mode (e.g. `:set digit-slot-mode per-workspace`).
- **CofiConfigSpec** — provider-owned config descriptor registered during provider bootstrap; config descriptors drive `:set`, save/load, and Config tab rows.
