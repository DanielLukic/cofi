# X11

## Purpose
X11 is cofi's direct Xlib/EWMH boundary: it reads window-manager state, sends
window-manager requests, enumerates windows/workspaces, and turns raw X events
into application refresh callbacks.

## Boundary

### Owns
- X11/EWMH property access, atom caching, window metadata extraction, workspace
  metadata, workarea queries, frame extents, size hints, and window state/client
  messages.
- Window enumeration into `WindowInfo`, including title/class/type/pid/desktop
  fields and filtering out cofi's own window.
- X11 event watch setup, root/per-window event dispatch, workspace-switch
  bookkeeping, and event-triggered AppData refresh callbacks.
- Raw monitor/window geometry helpers, frame-aware move/resize calls, and
  process-to-window lookup through `_NET_WM_PID` plus `/proc` ancestry.

### Does Not Own
- UI lifecycle, display rendering, filtering/ranking, matching semantics,
  geometry planning policy, rules definitions, or hotkey binding policy.
- Provider behavior, command behavior, slot behavior, or persistence formats.
- Window-manager compliance beyond best-effort Xlib/EWMH requests and fallbacks.

## Public Surface
- `atom_cache_init()` and `AtomCache`
- `WindowInfo`, `WorkspaceInfo`, `WorkArea`, `FrameExtents`,
  `WindowSizeHints`, and workspace helper types
- `get_window_list()`, X11 property/window/workspace/state helpers,
  intent-named state query helpers (`window_is_hidden()`,
  `window_is_shaded()`, `window_is_sticky()`, `window_is_fullscreen()`,
  `window_is_maximized_horizontal()`, `window_is_maximized_vertical()`),
  intent-named state mutation helpers (`set_window_maximized()`,
  `set_window_maximized_horizontal()`, `set_window_maximized_vertical()`,
  `set_window_fullscreen()`, `set_window_above()`, `set_window_below()`,
  `set_window_skip_taskbar()`, `set_window_sticky()`), and frame-aware
  move/resize helpers
- `move_window_to_next_monitor()`, `move_window_to_monitor_index()`, monitor
  move helpers, workarea, size-hint, frame-extent, process-window, and workspace
  utility functions
- `setup_x11_event_monitoring()`, `cleanup_x11_event_monitoring()`,
  `process_x11_events()`, `handle_x11_event()`, `update_current_workspace()`,
  and `set_workspace_switch_state()`

## Acceptance Criteria
1. Atom cache initialization interns the EWMH and standard atoms cofi reuses for
   window names, types, pids, desktops, active window, client list, workarea,
   WM state, and UTF-8 strings.
2. Generic property reads return `COFI_SUCCESS` only when X11 returns property
   data; callers receive ownership of successful `XGetWindowProperty` buffers.
3. Window property helpers read modern EWMH names before legacy fallbacks where
   applicable and return safe defaults when X11 properties are missing.
4. Window type classification returns `Normal` only when `_NET_WM_WINDOW_TYPE`
   is absent or contains only `_NET_WM_WINDOW_TYPE_NORMAL`; other type mixes are
   classified as `Special`.
5. Window list refresh reads `_NET_CLIENT_LIST`, validates windows, skips null
   IDs and cofi-owned windows, caps at `MAX_WINDOWS`, and fills title, instance,
   class, type, pid, and desktop fields.
6. Missing or empty window titles become `Untitled window` rather than excluding
   the window from downstream filtering.
7. Workspace helpers read current desktop, desktop count, desktop names, and
   `_NET_WORKAREA`, with fallback to one desktop, generated names, or full-screen
   workarea when EWMH data is unavailable.
8. Workspace key/argument resolution maps `1`-`9`, `0`, arrows, and HJKL to
   valid zero-based workspace indices and returns `-1` for invalid or edge moves.
9. Window state mutations use EWMH client messages for desktop switches,
   desktop moves, intent-owned state set/unset/toggle requests, close,
   minimize, and title updates, flushing requests before returning. Atom names
   stay inside x11; callers express state intent.
10. Full maximize uses one `_NET_WM_STATE` client message with the vertical and
   horizontal maximize atoms in `data.l[1]` and `data.l[2]`; toggle removes
   both only when both states are already present and otherwise sets both.
11. Frame-extents helpers read `_NET_FRAME_EXTENTS`, expose validity checks, and convert saved frame-space positions to client-space before `XMoveResizeWindow`.
12. Size-hint helpers apply minimum, maximum, base-size, and resize-increment
   constraints to requested rectangles before geometry callers use them.
13. Monitor move uses XRandR geometry, preserves maximized/tiled state, wraps to the next monitor, and keeps normal windows within target bounds.
14. Explicit monitor-index moves use zero-based XRandR monitor indices, preserve
   the same geometry/state behavior as next-monitor moves, and return false for
   negative or out-of-range indices without moving the window.
15. Process-window lookup first matches windows by `_NET_WM_PID`, then walks
   `/proc/<pid>/status` parent PIDs up to the requested depth.
16. Event monitoring selects root property/substructure events, watches the X11
   connection through GLib, subscribes current windows to `PropertyNotify`, and
   cleans up the GLib watch/channel on shutdown.
17. `_NET_CLIENT_LIST` events snapshot previous window ids, refresh AppData's
   window list, compute newly-added window ids for rule trigger gating, reassign
   live match entries, prune rule state for absent windows, refilter using
   current query semantics, and update visible UI only when the cofi window is
   present.
18. `_NET_ACTIVE_WINDOW` and `_NET_CURRENT_DESKTOP` events update active-window and workspace state, including highlight suppression or fallback timer behavior.
19. Per-window title changes update cached `WindowInfo` titles and re-evaluate
   matching rules without re-entering rule dispatch.
20. `_NET_FRAME_EXTENTS` changes re-run saved geometry restore for the affected
   window, relying on geometry planning idempotence for no-op cases.
21. `KeyPress` events are delegated to the hotkey dispatcher; x11 does not own
   the hotkey binding table or action semantics.

## Notes
- cofi intentionally uses direct Xlib/EWMH here; do not replace these paths with
  shell tools such as `wmctrl`.
- Window-list updates are event-driven from X11 notifications. Avoid polling
  unless a future ticket explicitly changes that architecture.
- Many higher-level subsystems depend on these headers. Keep this folder's API
  raw and policy-light; product behavior belongs above this boundary.
