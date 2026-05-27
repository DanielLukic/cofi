# Run Mode

## Purpose
Run mode launches arbitrary commands and exposes run history through the run modal/provider surface.

## Boundary

### Owns
- Run-mode command extraction from typed entry text and command arguments.
- Session-only run history ordering, capacity, latest-entry deduplication, and selection-to-entry sync.
- RUN modal provider registration, row formatting, row identity, and launch-on-enter behavior.
- Shell command launch dispatch through the detached shell launcher.
- The `run` command, `r` alias, `!` provider prefix, and run delegate/hotkey claim.

### Does Not Own
- Shell parsing, process detachment, terminal policy, or command execution semantics after calling `detach_launch_shell()`.
- Persistent storage of command history; RUN history is in-memory for the current cofi process only.
- Generic modal behavior, command parsing, provider registry, daemon dispatch, or hotkey grab implementation.
- Global selection movement beyond filling the entry when RUN history selection changes.
- Validation of whether a command exists before launching it.

## Public Surface
- `run/run_mode.h`
- `init_run_mode()`, `extract_run_command()`, `add_run_history_entry()`
- `run/run_provider.h`
- `run_provider_register()`

## Acceptance Criteria
1. `run_provider_register()` registers an optional hidden-by-default dynamic
   tab provider with id `run`, display name `RUN`, prefix character `!`, run
   delegate opcode, run hotkey-mode claim, clear-then-return modal policy,
   initial selection index `0`, and RUN row, selection, Enter, and command
   callbacks.
2. Registering the provider also registers the `run` command and `r` alias.
3. Invoking `run` or `r` with no arguments exits command mode, records the
   current tab as prefix origin, claims the `!` prefix, enters the RUN modal,
   and does not launch or hide.
4. Invoking `run COMMAND` or `r COMMAND` exits command mode, extracts the
   command, launches it through `detach_launch_shell()`, adds it to history on
   successful launch, hides cofi on success, and leaves cofi visible on
   failure or empty command.
5. Entering the RUN tab sets the entry placeholder to `command`.
6. `extract_run_command()` accepts raw command text or text prefixed with `!`,
   strips the prefix when present, trims surrounding whitespace, and rejects
   bare `!`, null, empty, or whitespace-only commands.
7. `init_run_mode()` clears all run state and initializes `history_index` to
   `-1`.
8. Run history is session-only, newest-first, capped at `RUN_HISTORY_CAP`, and
   not persisted across cofi restarts.
9. Adding a history entry ignores empty commands and skips insertion when the
   command already equals the newest history entry.
10. Adding a distinct command shifts older entries down, inserts the command at
    index `0`, increments history count up to capacity, and resets
    `history_index` to `-1`.
11. RUN row count equals the current history count; there is no placeholder row
    when history is empty.
12. Valid history rows render one actionable cell containing the command text;
    invalid row indices render an empty non-actionable row.
13. Row match text and row identity are both the history command string.
14. Changing selection to a valid history row fills the entry with that command
    and moves the caret to the end while suppressing recursive entry-change
    handling.
15. Changing selection to an invalid history row clears the entry without
    launching anything.
16. Pressing Enter with non-empty entry text extracts and launches that typed
    command, records it in history only on successful launch, and returns
    handled-hide on success.
17. Pressing Enter with empty entry text re-launches the selected history row
    when the row index is valid and does not duplicate that history entry.
18. Pressing Enter with empty entry text and no valid history row is a no-op.
19. Launch failures from `detach_launch_shell()` do not add history and return
    no-op to the provider.

## Notes
RUN intentionally launches through the shell. Do not add preflight command existence checks here unless the launch subsystem grows a shared contract for them.
