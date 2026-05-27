# Processes

## Purpose
Processes provides the process-list tab and process actions such as showing or signaling selected processes.

## Boundary

### Owns
- Process inventory collection from `/proc`, including pid, basename, command line, memory, and CPU samples.
- Filtering, ranking, row identity, and display formatting for the PROC tab.
- Process actions entered through the PROC tab, including selected/all signal actions and window-show actions.
- Action-token completion candidates after the `|` action separator.
- PROC tab lifecycle hooks: refresh on enter/tick, clear action candidates on leave, and query-driven filtering.
- The `proc` command and `ps` alias that surface the dynamic process tab.

### Does Not Own
- The generic provider registry, command registry, tab switching, or modal infrastructure.
- X11 window activation internals beyond requesting activation for a process-associated window.
- Global selection movement and scroll mechanics beyond preserving/resetting provider indices after PROC filters and refreshes.
- Kernel `/proc` availability, process permissions, or OS signal delivery semantics.
- Persistent storage; PROC state is refreshed from the live system and is not saved across cofi restarts.

## Public Surface
- `proc/proc.h`
- `ProcEntry`, `ProcCpuSample`, `ProcMode`
- `MAX_PROCS`, `MAX_PROC_BASENAME_LEN`, `MAX_PROC_CMDLINE_LEN`
- `init_proc_mode()`
- `proc_start_polling()`, `proc_stop_polling()`, `proc_refresh()`
- `proc_filter()`, `proc_update_action_candidates()`
- `proc_signal_selected_with_modifiers()`, `proc_execute_action_with_modifiers()`
- `proc_row_count()`, `proc_format_row()`, `proc_match_string()`, `proc_row_identity()`
- `proc_on_enter()`, `proc_on_leave()`, `proc_on_query_changed()`, `proc_on_tick()`
- `proc_format_mem_compact()`, `proc_format_cpu_pct()`
- `proc_fit_name_column()`, `proc_fit_cmd_column()`
- `proc/proc_provider.h`
- `proc_provider_register()`

## Acceptance Criteria
1. `proc_provider_register()` registers an optional hidden-by-default dynamic
   tab provider with id `proc`, display name `PROC`, hide-on-Esc modal policy,
   initial selection index `0`, 1500 ms tick refresh, and PROC row, query,
   lifecycle, and action callbacks.
2. Registering the provider also registers the `proc` command and `ps` alias;
   invoking either exits command mode, records the current tab as prefix
   origin, surfaces the PROC tab, and ignores non-empty command arguments
   after warning.
3. Entering the PROC tab sets the entry placeholder to `Processes...` and
   immediately refreshes the process inventory.
4. Leaving the PROC tab clears command completion candidates and stops PROC
   polling.
5. A refresh reads up to `MAX_PROCS` numeric `/proc` entries with non-empty
   command lines, records basename, command line, RSS fallbacking to virtual
   size, and CPU percentage from jiffy deltas.
6. The unfiltered process list is sorted by memory descending, with pid
   ascending as the stable tie-breaker.
7. Refreshes avoid repainting when the pid-and-basename snapshot is unchanged,
   even if command line or RSS changed.
8. Refreshes and filter changes preserve the selected process by pid when it
   remains visible; otherwise selection and provider scroll reset to the first
   row.
9. Empty fuzzy filters show every loaded process in inventory order.
10. Fuzzy filters rank basename matches above command-line-only matches by
    weighting basename scores, then sort ties by RSS descending and pid
    ascending.
11. A filter ending in `!` uses strict substring mode: exact basename matches
    rank first, basename substring matches second, command-line substring
    matches third, and fuzzy-only hits are excluded.
12. A filter ending in `$` uses exact mode: only case-insensitive basename
    equality matches are included, sorted by RSS descending and pid ascending.
13. Only the final suffix character selects strict or exact mode; earlier `!`
    or `$` characters remain part of the literal filter text.
14. A `|` separator splits the entry into filter text and action text using
    the last pipe; both sides are trimmed before filtering or action parsing.
15. While typing an action after `|`, PROC publishes command candidates
    matching the action prefix, including compact `all` forms such as `ka`.
16. Pressing Enter without a valid pipe action signals the selected process:
    plain Enter sends `SIGTERM`, Shift+Enter sends `SIGKILL`, and Ctrl+Enter
    sends `SIGHUP`.
17. Pipe actions resolve `k`/`kill`/`term`/`t` to `SIGTERM`, `9`/`kill9`/`force`
    to `SIGKILL`, `h`/`hup` to `SIGHUP`, `stop` to `SIGSTOP`, `c`/`cont` to
    `SIGCONT`, and `s`/`show`/`w` to window-show.
18. Action scope defaults to the selected row; `all` or compact tokens ending
    in `a` apply signal actions to every currently filtered process.
19. Unsupported or malformed actions do not signal processes, do not hide
    cofi, and return no-op to the provider.
20. Successful signal actions refresh the inventory, clear any PROC error,
    hide cofi, and return handled-hide to the provider.
21. Signal failures set a user-visible error for permission, missing-process,
    or generic failure cases and refresh the display instead of hiding cofi.
22. Show actions search for a window owned by the selected pid or one of its
    ancestors up to the depth limit, activate the found window, and hide cofi;
    `show all` is unsupported.
23. If no process inventory can be displayed, PROC shows a single loading,
    no-results, no-processes, or error row rather than exposing stale rows.
24. Normal process rows render five cells: pid, CPU percentage, compact memory,
    fitted basename, and fitted command line; actionable rows use pid string
    identity and command line match text.
25. Memory formatting reports sub-1 MiB values as `0M`, MiB values with
    compact `M`, and GiB values with compact `G`; CPU formatting clamps
    negatives to `0.0` and prints one decimal place.
26. Basename and command-line display text is trimmed, padded when short, and
    truncated with ellipsis when it exceeds the configured column width.

## Notes
PROC is Linux `/proc`-specific. It is intentionally live-only state: refreshes should prefer accurate current system data over persistence or cached command metadata.
