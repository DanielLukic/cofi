# System Actions

## Purpose
System actions run user-facing operating-system actions that are not specific to one cofi tab.

## Boundary

### Owns
- The fixed catalog of user-facing system actions exposed as `AppEntry` records.
- Search metadata for those actions, including display names, keywords, source kind, and action ids.
- Invocation routing for lock, suspend, hibernate, logout, reboot, and shutdown actions.
- logind D-Bus calls and lock-command fallback ordering used to request OS-level actions.
- Failure logging when system action invocation cannot be dispatched.

### Does Not Own
- The APPS provider, filtering, row display, or launch selection mechanics that surface system actions.
- OS policy, authorization prompts, inhibitor handling, or whether logind ultimately performs the action.
- Desktop environment screensaver implementation beyond trying known lock commands.
- Command mode, daemon delegation, hotkeys, or global shutdown confirmation UI.
- Persistent configuration; the system action catalog is static.

## Public Surface
- `system_actions/system_actions.h`
- `system_actions_load()`, `system_actions_invoke()`

## Acceptance Criteria
1. `system_actions_load()` exposes exactly six actions in deterministic order:
   Lock, Suspend, Hibernate, Logout, Reboot, and Shutdown.
2. Loading respects the caller-provided `max`; if `max` is smaller than the
   catalog, only the first `max` actions are written and counted.
3. Passing null output, null count, or non-positive `max` produces no entries;
   when a count pointer is provided, it is reset to `0`.
4. Every loaded entry has `source_kind == APP_SOURCE_SYSTEM`, a non-empty
   null-terminated name, `info == NULL`, and a non-`SYSTEM_ACTION_NONE`
   `action_id`.
5. Loaded entries keep `generic_name` empty and fill `keywords` with searchable
   synonyms such as `session` for Lock, `standby` for Suspend, and `poweroff`
   for Shutdown.
6. Repeated loads with the same capacity produce identical counts, names,
   keywords, source kinds, and action ids.
7. `system_actions_invoke()` returns immediately for null entries or entries
   whose `source_kind` is not `APP_SOURCE_SYSTEM`.
8. Unknown system action ids are logged as warnings and do not attempt D-Bus or
   command execution.
9. Lock first attempts, in order, `xdg-screensaver lock`,
   `mate-screensaver-command --lock`, `xscreensaver-command -lock`, and
   `loginctl lock-session`.
10. If lock commands are unavailable, Lock falls back to logind
    `org.freedesktop.login1.Session.Lock`, trying `/session/auto` first and
    then `/session/$XDG_SESSION_ID`.
11. Suspend, Hibernate, Reboot, and Shutdown call logind manager methods
    `Suspend`, `Hibernate`, `Reboot`, and `PowerOff` respectively, each with
    the interactive boolean set to `TRUE`.
12. Logout requires `XDG_SESSION_ID` and calls logind manager
    `TerminateSession` for that session id.
13. D-Bus proxy creation failures, D-Bus method failures, missing
    `XDG_SESSION_ID` for logout/lock fallback, and unavailable lock commands
    are logged and reported as failed invocations without crashing cofi.
14. Successful or failed recognized invocations log timing with the action
    name and success flag.

## Notes
System actions are surfaced through APPS as launchable rows. Any new action must be safe to expose in that context and should include searchable synonyms matching likely user queries.
