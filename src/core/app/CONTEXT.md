# Core App

## Purpose
Core app owns process startup, top-level application wiring, and shared runtime state for the cofi binary.

## Boundary

### Owns
- The `AppData` aggregate that carries process-wide mutable state between subsystems.
- The top-level `run_cofi()` startup, delegation, daemon binding, GTK main-loop, and shutdown sequence.
- Application shell construction: the top-level GTK window, text view, entry, overlay root, and event signal wiring.
- Baseline initialization of shared state fields before feature subsystems use them.
- Startup ordering between CLI parsing, provider registration, X11 connection, config load, data enumeration, UI setup, hotkeys, and daemon monitoring.
- The tiny C entrypoint that delegates `main()` to `run_cofi()`.

### Does Not Own
- The behavior of feature subsystems stored inside `AppData`, such as matching, rules, harpoon, projects, calc, or sinks.
- Window filtering, ranking, activation semantics, or provider row behavior beyond invoking their initialization hooks in order.
- The daemon socket wire format, hotkey binding model, config schema, or X11 event processing internals.
- Command-line option parsing semantics beyond acting on the parsed `AppData` and setup outputs.
- Rendering row contents, tab-specific UI behavior, or overlay business logic after the shell is wired.

## Public Surface
- `core/app/app_data.h`
- `core/app/app_init.h`
- `core/app/app_setup.h`
- `AppData`, `TabMode`, `TabVisibility`, `OverlayType`, `CommandMode`, `RunMode`, `SelectionState`
- `init_tab_visibility()`, `apply_provider_default_visibility()`
- `init_app_data()`, `init_x11_connection()`, `init_workspaces()`
- `init_window_list()`, `init_history_from_windows()`
- `setup_application()`, `on_textview_size_allocate_for_fixed_init()`
- `run_cofi()`

## Acceptance Criteria
1. `main()` delegates all process behavior to `run_cofi()` and returns its
   exit status unchanged.
2. `run_cofi()` initializes config defaults, parses CLI flags, handles help
   and parse-failure exits, and applies logging before subsystem startup.
3. When another daemon is already listening and a startup delegate opcode is
   present, `run_cofi()` sends the requested opcode or `--show` tab name to
   the existing daemon and exits without starting a second UI process.
4. When another daemon is already listening and no delegate opcode is present,
   `run_cofi()` reports that cofi is already running and exits with failure.
5. For normal daemon startup, `run_cofi()` binds the daemon socket before GTK
   initialization, stores the listener in `AppData`, and arms exit/signal
   cleanup so the socket path is removed on shutdown.
6. Startup order after `gtk_init()` is: initialize `AppData` including its
   early persisted-state loads, register builtin providers, open X11 and
   initialize atoms, load config, apply disabled providers, apply provider
   default tab visibility, load remaining persisted feature state for
   harpoon slots, layout store, and geometry rule sync, enumerate windows and
   workspaces, initialize history and selection, build the GTK shell, monitor
   X11 events, realize the window, record the own-window XID, set up hotkeys,
   start daemon monitoring, then dispatch any startup delegate before entering
   `gtk_main()`.
7. `init_app_data()` resets shared runtime fields to safe defaults, preserves
   a valid startup provider tab selected by CLI parsing, and falls back to
   `TAB_WINDOWS` for invalid tab values.
8. `init_app_data()` also performs early disk I/O before X11 is opened: it
   loads hotkey config with default fallback and missing-file save behavior,
   loads match entries, loads rules config, immediately re-saves match entries
   after legacy rule migration, and loads calculator history.
9. `init_tab_visibility()` starts every tab hidden except the Windows tab,
   which is pinned; `apply_provider_default_visibility()` pins enabled
   providers that are not hidden by default.
10. `init_x11_connection()` exits the process if the X display cannot be
   opened, and initializes the atom cache immediately after a successful
   connection.
11. `init_window_list()` enumerates current windows and persists matching
    reassignments when live windows cause match entries to bind differently.
12. `init_history_from_windows()` seeds history from the current window list,
    then runs the Windows filter with an empty query so filtered rows reflect
    history ordering.
13. `setup_application()` creates the undecorated always-on-top GTK shell,
    attaches the text view, entry, mode indicator, overlay root, CSS, focus
    settings, and key/change/focus/delete signal handlers used by the rest of
    the app.
14. The fixed-size one-shot textview allocation handler initializes fixed
    window sizing once, disconnects itself, and performs a pending initial
    render only after sizing authority is established.
15. `--assign-slots` mode initializes enough state to enumerate windows and
    workspaces, assigns workspace slots, closes the log file when present, and
    exits before history, UI, X11 event monitoring, hotkeys, daemon watch, or
    `gtk_main()`.
16. Normal shutdown after `gtk_main()` cleans up hotkeys, daemon monitoring,
    daemon socket cleanup registration, signal handlers, window highlight,
    X11 event monitoring, the X display connection, and any opened log file.

## Notes
`AppData` is a shared state container, not a license for cross-subsystem reach-through. New feature behavior should still live behind the owning subsystem's API and only add fields here when state truly must be process-wide.

The startup sequence in `run_cofi()` is an observable, order-sensitive
contract. Persisted feature state such as harpoon slots, layouts, and geometry
rule sync loads after config and provider enablement; window enumeration must
happen after the X11 connection is open.

`main.c` is the only process entry point. All execution paths route through
`run_cofi()`.

`--assign-slots` skips GTK window creation, X11 event monitoring, hotkeys, and
`gtk_main()` entirely. It is a one-shot batch mode, not a UI mode.
