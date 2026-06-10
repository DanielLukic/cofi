# Commands

## Purpose
Commands provide cofi's typed action language: parsing command text, resolving
aliases, registering built-in and provider commands, dispatching handlers, and
exposing help/candidate metadata for users.

## Boundary

### Owns
- The command registry: primary names, aliases, compact suffix metadata,
  provider/core ownership, activation policy, hotkey keep-open policy, and
  duplicate-name rejection.
- Command parsing and segmentation for typed commands, hotkey command strings,
  and comma-separated rule command lists.
- Dispatch policy for executing one or more command segments against the
  selected window, an explicit hotkey target window, or a rule/background
  target.
- The built-in command table and handler entrypoints for core window,
  workspace, tiling, layout, naming, config, help, and tab-surface commands.
- Command availability filtering for provider-owned commands when their owning
  provider is disabled.
- Command help text generation and command-mode candidate matching data.

### Does Not Own
- Feature behavior invoked by handlers, such as X11 primitives, geometry
  planning, Harpoon slot storage, matching entries, config persistence, or
  provider tab behavior.
- The visual command-mode UI, keybinding policy, display rendering, overlay
  hosting, or tab lifecycle; commands supply state/helpers consumed by UI code.
- Provider-specific command handlers registered outside the built-in command
  table.
- CLI argument parsing policy beyond exposing command registry/help functions
  consumed by the CLI layer.

## Public Surface
- `CommandSpec`, `CommandHandler`, `COMMAND_OWNER_CORE`, and `HelpFormat`
- `CommandSpec.closes_cofi_after_execute` as the typed-command dismiss policy flag
- `cofi_register_command()`, `cofi_command_registry_reset()`,
  `cofi_command_count()`, `cofi_command_at()`,
  `cofi_command_by_primary()`, and `cofi_command_for_token()`
- `cofi_register_core_commands()`
- `trim_whitespace_in_place()`, `parse_command_and_arg()`,
  `parse_command_for_execution()`, `resolve_command_primary()`,
  `next_command_segment()`, and `visit_command_segments()`
- `execute_command()`, `execute_command_with_window()`,
  `execute_command_background()`, `should_keep_open_on_hotkey_auto()`, and
  `should_close_after_execute()`
- `command_primary_is_available()`
- `CommandMode`, `init_command_mode()`, `enter_command_mode()`,
  `exit_command_mode()`, `handle_command_key()`,
  `command_update_candidates()`
- Built-in handlers declared in `command_handlers_window.h`,
  `command_handlers_workspace.h`, `command_handlers_tiling.h`, and
  `command_handlers_ui.h`
- `cofi_surface_provider_command()` and `generate_command_help_text()`

## Acceptance Criteria
1. Registering a command requires a non-null spec, primary name, owner provider
   id, and available registry capacity; invalid specs fail without changing the
   registry.
2. The registry rejects duplicate primary names and duplicate aliases across all
   registered commands, while preserving registration order for successful
   commands.
3. `cofi_command_for_token()` resolves both primary names and aliases, whereas
   `cofi_command_by_primary()` resolves only exact primary names.
4. `cofi_command_registry_reset()` clears all registered command slots and
   returns the command count to zero.
5. Core commands register only through `cofi_register_core_commands()` and use
   `COMMAND_OWNER_CORE` so availability does not depend on provider enablement.
6. Provider-owned commands are available only when their owning provider exists
   and is enabled; disabled provider commands are hidden from dispatch,
   candidates, help, and hotkey keep-open policy.
7. Command parsing trims surrounding whitespace, splits the first whitespace
   separated token as the command name, and preserves the remaining text as the
   argument after trimming.
8. Compact command parsing expands registered compact suffix forms such as
   workspace, tiling, and state-toggle commands only when the full token is not
   already an exact command or alias.
9. Compact parsing chooses the longest matching registered command/alias prefix
   before splitting suffix text into the argument.
10. `parse_command_for_execution()` resolves aliases to their primary command
    name but leaves unknown commands unchanged so callers can report them.
11. Command segmentation treats commas as separators, trims whitespace around
    each segment, skips empty segments, visits segments in stored order, and
    returns false immediately when a segment visitor returns false.
12. `execute_command()` logs user execution and dispatches segments against the
    current selected window.
13. `execute_command_with_window()` logs hotkey execution and dispatches against
    the explicit target window supplied by the hotkey path.
14. `execute_command_background()` logs rule execution, dispatches against the
    supplied rule target window, and suppresses post-command activation even for
    activating commands.
15. Dispatch stops and returns false when parsing fails, when the resolved
    command is unavailable, or when no handler is registered for the command.
16. A command handler returning true plus `CommandSpec.activates` reactivates
    the commanded window for foreground user/hotkey dispatch.
17. `should_keep_open_on_hotkey_auto()` returns true when any segment is an
    available command marked `keeps_open_on_hotkey_auto`, or an available command
    marked `keeps_open_without_arg` with no argument.
18. `should_close_after_execute()` returns true when any segment is an available
    command marked `closes_cofi_after_execute`.
19. Command-mode candidates include available primary names and aliases matching
    the typed prefix, strip compact suffix noise from candidate matching, and
    omit disabled provider commands.
20. Command-mode history keeps the most recent command first, suppresses
    consecutive duplicates, caps stored history at ten entries, and restores it
    into newly initialized command-mode state.
21. Entering command mode clears the command buffer, cursor, candidates, help
    scroll, and target selection state while preserving a pre-focused hotkey
    target when it still exists in the filtered window list.
22. Exiting command mode resets command-mode state, clears command targeting,
    restores normal mode, and hides cofi only when `close_on_exit` was set.
23. Command-mode Enter executes the highlighted candidate when present,
    otherwise executes the typed command buffer; on successful execution it
    exits command mode, dismisses cofi only when any executed segment is marked
    `closes_cofi_after_execute`, and on failed execution keeps command mode open
    with the line cleared.
24. Escape in command mode exits command mode immediately and unconditionally;
    there is no clear-then-exit two-stage behavior.
25. Up and Down in command mode navigate command-input history regardless of
    whether command candidates are visible.
26. Left and Right wrap the candidate highlight when candidates are visible;
    otherwise they behave as normal cursor motion in the input field.
27. Built-in window-state commands (`ew`, `sb`, `aot`, `ab`, `mw`, `hmw`,
    `vmw`) accept empty/`toggle`, `on`/`+`, and `off`/`-` arguments and reject
    any other state argument without changing the target window state.
28. Built-in window commands fail cleanly when no selected target window is
    available, except commands that intentionally surface an overlay or operate
    without a window.
29. `tm` with no argument moves the target window to the next monitor; with a
    numeric argument it moves to that zero-based monitor index, and invalid or
    out-of-range monitor arguments log a warning and return false.
30. Workspace commands resolve numeric and configured grid-direction arguments
    through x11 workspace helpers; missing arguments surface the appropriate
    workspace overlay instead of guessing.
31. Tiling commands apply a parsed tile option when an argument is present and
    surface the tiling overlay when no option is supplied.
32. Config `set` validates `key=value` or `key value`, accepts quoted empty
    values, saves config on success, applies disabled-provider changes
    immediately, and surfaces the Config tab.
33. Help generation includes static navigation/tab/prefix guidance plus
    available grouped commands, wraps long descriptions to the requested width,
    and emits unwrapped text when width is zero or invalid.
34. `cmd_assign_name` requires a selected window and the Windows tab; a
    non-empty (whitespace-trimmed) inline label delegates to
    `names_assign_window()`, while empty input opens the name-assignment
    overlay; typed-command dismissal for the inline path is owned by the
    command dispatcher via `closes_cofi_after_execute`, not by the handler.

## Notes
- `command_mode.*` currently lives here because it is named `command_*` and
  stores command buffer/candidate/history mechanics. Its visual rendering and
  top-level key routing remain UI concerns.
- Command handlers are intentionally thin adapters. If a handler grows feature
  policy, move that policy to the owning feature subsystem and keep the command
  as a dispatch surface.
- `cmd_assign_name` delegates name persistence to `names_assign_window()` in
  `src/names/`; the command handler holds no name-persistence logic of its own.
- Known exceptions to the thin-adapter intent exist today: `cmd_mouse()` in
  `command_handlers_tiling.c` performs direct X11 pointer/cursor work, and
  `cmd_swap_windows()` in `command_handlers_window.c` contains raw X11 geometry
  sequencing. Treat these as cleanup candidates, not patterns to copy into new
  command handlers.
- Window-state commands express product intent through x11 helpers; raw
  EWMH state atom names live in `x11/`, not in command handlers.
- Provider command ownership is the boundary that lets disabled tabs remove
  their commands without deleting the command specs from the registry.
