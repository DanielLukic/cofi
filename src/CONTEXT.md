# Source Tree

## Purpose
`src/` contains the production code for the cofi binary. It is organized by
subsystem so contributors can find the product capability they are changing
without relying on a flat source-file inventory.

## CONTEXT.md Convention
Every folder under `src/` that contains source or header files must also contain
a `CONTEXT.md`. The convention is recursive: nested source folders need their
own local context files.

Subsystem `CONTEXT.md` files should explain the "what" more than the "how":
purpose, ownership boundaries, public surface, behavior-oriented acceptance
criteria, and only essential notes or gotchas. Numbered acceptance criteria are
future `.spec` seeds.

See `CLAUDE.md` for the contributor contract and the full recursive
`CONTEXT.md` convention.

## Subsystems
- `apps/` - desktop app discovery and provider tab.
- `calc/` - calculator evaluation and provider surface.
- `cli/` - command-line argument parsing, help/version output, and startup delegation flags.
- `core/app/` - application state, startup wiring, setup, and entrypoint.
- `core/history/` - window MRU ordering and Alt-Tab two-slot preservation.
- `core/json/` - shared tolerant JSON persistence helpers.
- `core/log/` - process-wide logging macros and log implementation.
- `core/nav_keys/` - shared keyboard navigation classification.
- `core/repeat_action/` - last-action repeat behavior.
- `core/selection/` - shared row selection movement and validation.
- `core/slot_store/` - shared key/payload slot assignment persistence infrastructure.
- `core/utils/` - shared utility helpers, UTF-8 display columns, constants, common types, and the embedded arithmetic evaluator.
- `commands/` - command registry, parser, dispatch, built-in command set, and command-mode mechanics.
- `config/` - base config defaults, persistence, validation, registry, and Config provider tab.
- `daemon/` - Unix-socket delegation, global hotkeys, hotkey configuration overlays, and detached process launching.
- `emoji/` - emoji data, ranking, history, and provider tab.
- `geom/` - saved layouts, geometry restore planning, tiling, and geom-rule synchronization.
- `harpoon/` - persistent window slots, provider slot key handling, and per-workspace visible-window digit slots.
- `matching/` - fuzzy ranking, match entries, custom names, pattern editing, and matching garbage collection.
- `proc/` - process-list provider and process actions.
- `profiles/` - browser profile discovery and provider tab.
- `projects/` - tmux/zellij sessions, project folders, remote project scope, and PATH binary cache.
- `providers/` - shared provider interface, registry, filtered maps, and built-in registration order.
- `run/` - run modal, command launching, and run provider.
- `rules/` - saved window rules, evaluation state, replay, and rules provider overlays.
- `sessions/` - session discovery, provider tab, and session overlay.
- `sinks/` - audio sink discovery, filtering, and provider tab.
- `system_actions/` - shared OS-level user actions.
- `ui/` - GTK popup surface, display rendering, tab/prefix routing, overlays, and visual affordances.
- `workspaces/` - workspace provider and workspace overlay.
- `x11/` - direct Xlib/EWMH primitives, window enumeration, workspace metadata, and event dispatch.
