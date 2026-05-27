# Applications

## Purpose
Applications provides desktop application discovery, matching, and launch behavior for the apps tab.

## Boundary

### Owns
- Desktop application discovery through GIO and filtering for the APPS tab.
- Inclusion and launch dispatch of system actions in the APPS result set.
- Apps-tab provider registration, row formatting, row identity, and launch-on-enter behavior.
- Apps mode switching between default desktop/system results and PATH-binary results.
- The `apps` command and aliases that surface the APPS tab.

### Does Not Own
- PATH binary discovery, caching, monitors, or ranking; APPS only routes `$` prefix queries to `path_binaries`.
- System action definitions or execution internals beyond including and invoking their `AppEntry` records.
- Desktop entry parsing policy outside the GIO `GAppInfo`/`GDesktopAppInfo` data exposed to this subsystem.
- Generic provider registry, command registry, tab switching, daemon delegation, or selection mechanics.
- Process-group detachment implementation beyond choosing the correct detach-launch helper for an app entry.

## Public Surface
- `apps/apps.h`
- `AppEntry`, `AppSourceKind`, `SystemActionId`
- `MAX_APPS`
- `apps_load()`, `apps_unload()`
- `apps_filter_entries()`, `apps_sort_entries()`
- `apps_filter()`, `apps_launch()`
- `apps/apps_provider.h`
- `apps_provider_register()`, `apps_tab_mode()`, `filter_apps()`

## Acceptance Criteria
1. The APPS tab is visible by default, accepts `$` and `\` prefixes, defaults
   to desktop+system mode, and switches to PATH-binary mode on `$` prefix.
   Entering the tab reloads entries and selects the first row; pressing Enter
   launches the selected actionable row.
2. Registering the provider also registers the `apps` command with
   `applications` and `app` aliases.
3. Invoking the `apps` command exits command mode, records the current tab as
   prefix origin, resets apps mode to default, surfaces the APPS tab, and
   returns without launching anything.
4. Entering the APPS tab sets the placeholder to
   `Type to filter applications...`, reloads desktop/system entries, and
   filters with an empty query.
5. Surfacing or leaving the APPS tab resets `apps_mode` to default; a `$` tab
   prefix switches to PATH mode, while other APPS prefixes keep default mode.
6. Default-mode filtering reads from the loaded desktop/system app list; PATH
   mode ensures the PATH cache is loaded and delegates filtering to
   `path_binaries`.
7. Query changes always refresh `filtered_apps` through the current mode and
   reset selection.
8. `apps_load()` unloads prior GIO resources, loads visible desktop apps with
   non-empty names up to `MAX_APPS`, appends system actions if capacity
   remains, sorts the combined list alphabetically by display name, and logs
   load timing.
9. `apps_unload()` releases the owned GIO app list and resets the loaded app
   count to zero.
10. Empty or null queries return every source entry in its existing order.
11. Non-empty filtering scores each app independently by name, generic name,
    and keywords; matches never span across fields.
12. Name matches outrank generic-name matches, which outrank keyword matches.
13. Token-prefix matches within a field outrank weaker fuzzy matches in that
    same field.
14. Keyword and generic-name matching is tokenized, so subsequences cannot
    cross separators such as spaces, semicolons, commas, slashes, dashes,
    underscores, periods, parentheses, or colons.
15. Filtered non-empty results sort by descending score, then alphabetically by
    app name for stable tie-breaking.
16. Row count includes all filtered apps, adds a `Scanning PATH...` status row
    while PATH scanning is active, and returns one `No matching applications
    found` row when there are no filtered apps.
17. Normal app rows render two cells: app name with width hint `48` and generic
    name with width hint `40`; only normal app rows are actionable.
18. Row match text is the app display name.
19. Row identity is source-specific: `path:<exec_path>` for PATH entries,
    `system:<action_id>` for system actions, and `desktop:<desktop-id-or-name>`
    for desktop apps.
20. Pressing Enter on a real app row launches that entry and returns
    handled-hide; pressing Enter on status/no-match rows is a no-op.
21. System action entries dispatch to `system_actions_invoke()`.
22. PATH entries launch through the terminal detach helper using `exec_path`.
23. Desktop entries strip desktop Exec field codes before launch, honor
    `Terminal=true` by launching through the terminal helper, and otherwise
    parse the command line into argv before detached launch.
24. Desktop launch failures for missing command lines, empty stripped commands,
    parse errors, or detach failures are logged and do not crash cofi.

## Notes
APPS is the user-facing aggregator for desktop apps, system actions, and `$` PATH mode. Keep PATH scanning behavior documented in the owning PATH subsystem; APPS should only describe how it routes to that mode.

`SystemActionId` is defined in `apps.h`, but system action behavior is
implemented in `system_actions/`. Adding a system action requires coordinated
changes in both subsystems.

The `$` prefix switches to PATH-binary mode; `\` keeps default desktop+system
mode. This asymmetry is intentional: `\` means desktop applications, while `$`
means search executables on `PATH`.
