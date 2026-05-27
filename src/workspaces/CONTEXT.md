# Workspaces

## Purpose
Workspaces provides the workspace tab and workspace-selection overlays for switching desktop context.

## Boundary

### Owns
- The WORKSPACES provider surface: filtering, row formatting, row identity, selection clamping, and switch-on-enter.
- Registration of the `workspaces` command, `ws` alias, workspace delegate opcode, and workspace hotkey-mode claim.
- The workspace rename overlay content, including current-name display, rename entry setup, and Enter-to-save handling.
- User-facing workspace names as edited through the rename overlay.

### Does Not Own
- Workspace enumeration, current-workspace detection, or storage in `AppData`; app initialization and X11 utilities populate that state.
- Low-level EWMH/X11 calls for switching desktops or writing desktop names.
- Digit-key workspace shortcuts, harpoon workspace slots, or geometry workspace switching outside the WORKSPACES provider.
- Generic provider registry, command registry, tab switching, daemon dispatch, hotkey dispatch, or overlay shell mechanics.
- Workspace creation/deletion; this subsystem switches and renames existing workspaces only.

## Public Surface
- `workspaces/workspaces_provider.h`
- `workspaces_tab_mode()`, `workspaces_provider_register()`
- `filter_workspaces()`, `workspaces_selected_workspace()`
- `workspaces/overlay_workspace.h`
- `create_workspace_rename_overlay_content()`
- `handle_workspace_rename_key_press()`

## Acceptance Criteria
1. `workspaces_provider_register()` registers an optional hidden-by-default
   dynamic tab provider with id `workspaces`, display name `WORKSPACES`,
   workspace delegate opcode, workspace hotkey-mode claim, and WORKSPACES row,
   query, entry, and Enter callbacks.
2. Registering the provider also registers the `workspaces` command and `ws`
   alias.
3. Invoking the `workspaces` command exits command mode, records the current
   tab as prefix origin, surfaces the WORKSPACES tab, and returns without
   switching workspaces.
4. `workspaces_tab_mode()` returns the registered dynamic tab mode when
   available, otherwise `TAB_WINDOWS`.
5. Entering the WORKSPACES tab sets the entry placeholder to
   `Type to filter workspaces...` and filters with an empty query.
6. Empty workspace filters include every workspace currently loaded in
   `AppData` order.
7. Non-empty workspace filters match against the one-based workspace number
   and workspace name, formatted as `<id + 1> <name>`.
8. Query changes re-filter workspaces and reset provider selection.
9. Row count returns the filtered workspace count when non-empty, otherwise a
   single status row.
10. Status rows display `No matching workspaces found` and are not actionable.
11. Normal workspace rows render three cells: current marker (`*` or blank),
    one-based workspace number in brackets, and workspace name.
12. Normal workspace rows are actionable.
13. Row match text is `<id + 1> <name>` and row identity is `workspace:<id>`.
14. `workspaces_selected_workspace()` clamps negative selection to the first
    row and overlarge selection to the last filtered row, writes the clamped
    index back to selection state, and returns null when there are no filtered
    workspaces.
15. Pressing Enter on a valid workspace row calls `switch_to_desktop()` with
    the zero-based workspace id and returns handled-hide.
16. Pressing Enter on a status or invalid row is a no-op.
17. The rename overlay header currently shows the zero-based workspace index,
    pre-fills the entry with the existing desktop name (`Unnamed` if missing),
    and accepts up to 64 characters. TFD-825 tracks fixing the header to show
    the one-based number.
18. The rename overlay stores the zero-based workspace index and entry widget
    on the overlay container, then focuses and selects the entry on idle only
    while the workspace-rename overlay remains active.
19. Pressing non-Enter keys in the rename overlay falls through.
20. Pressing Enter with a missing entry/index or an empty name is handled
    without calling `set_desktop_names()`.
21. Pressing Enter with a non-empty name loads existing desktop names, creates
    default `Desktop N` names when none are available, replaces the selected
    workspace name, and writes the full name list through `set_desktop_names()`.

## Notes
Workspace ids are zero-based internally and one-based in user-facing labels. Preserve that distinction in provider rows, overlay copy, and calls to X11 helpers.
