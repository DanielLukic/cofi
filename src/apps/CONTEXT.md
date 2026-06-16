# Applications

## Purpose
Applications provides desktop application discovery, matching, and launch behavior for the apps tab.

## Boundary

### Owns
- Desktop application discovery through GIO and filtering for the APPS tab.
- Inclusion and launch dispatch of system actions in the APPS result set.
- Apps-tab provider registration, row formatting, row identity, and launch-on-enter behavior.
- The `apps` command and aliases that surface the APPS tab.

### Does Not Own
- PATH executable discovery, caching, monitors, ranking, or launch behavior; those live in `src/path/`.
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
- `apps_provider_register()`, `apps_tab_mode()`

## Acceptance Criteria
1. The APPS tab is visible by default, accepts the `\` prefix, and shows only
   desktop applications plus fixed system actions. Entering the tab reloads
   entries and selects the first row; pressing Enter launches the selected
   actionable row.
2. Registering the provider also registers the `apps` command with
   `applications` and `app` aliases.
3. Invoking the `apps` command exits command mode, records the current tab as
   prefix origin, surfaces the APPS tab, and returns without launching
   anything.
4. Entering the APPS tab sets the placeholder to
   `Type to filter applications...`, reloads desktop/system entries, and
   filters with an empty query.
5. Query changes always refresh `filtered_apps` from the loaded
   desktop/system app list and
   reset selection.
6. `apps_load()` unloads prior GIO resources, loads visible desktop apps with
   non-empty names up to `MAX_APPS`, appends system actions if capacity
   remains, sorts the combined list alphabetically by display name, and logs
   load timing.
7. `apps_unload()` releases the owned GIO app list and resets the loaded app
   count to zero.
8. Empty or null queries return every source entry in its existing order.
9. Non-empty filtering scores each app independently by name, generic name,
    and keywords; matches never span across fields.
10. Name matches outrank generic-name matches, which outrank keyword matches.
11. Token-prefix matches within a field outrank weaker fuzzy matches in that
    same field.
12. Keyword and generic-name matching is tokenized, so subsequences cannot
    cross separators such as spaces, semicolons, commas, slashes, dashes,
    underscores, periods, parentheses, or colons.
13. Filtered non-empty results sort by descending score, then alphabetically by
    app name for stable tie-breaking.
14. Row count includes all filtered apps and returns one `No matching
    applications found` row when there are no filtered apps.
15. Normal app rows render two cells: app name with width hint `48` and generic
    name with width hint `40`; only normal app rows are actionable.
16. Row match text is the app display name.
17. Row identity is source-specific: `system:<action_id>` for system actions,
    and `desktop:<desktop-id-or-name>`
    for desktop apps.
18. Pressing Enter on a real app row launches that entry and returns
    handled-hide; pressing Enter on status/no-match rows is a no-op.
19. System action entries dispatch to `system_actions_invoke()`.
20. Desktop entries strip desktop Exec field codes before launch, honor
    `Terminal=true` by launching through the terminal helper, and otherwise
    parse the command line into argv before detached launch.
21. Desktop launch failures for missing command lines, empty stripped commands,
    parse errors, or detach failures are logged and do not crash cofi.

## Notes
APPS is the user-facing launcher for desktop apps and system actions only.
PATH scanning and launch behavior live in `src/path/`.

`SystemActionId` is defined in `apps.h`, but system action behavior is
implemented in `system_actions/`. Adding a system action requires coordinated
changes in both subsystems.

The `\` prefix is the only Apps-specific tab prefix.
