# UI

## Purpose
The UI subsystem owns cofi's GTK-facing surface: the popup window, rendered
text, tab header, key-dispatch shell, modal/prefix routing, overlays, and
short-lived visual affordances.

## Boundary

### Owns
- GTK widget helpers, popup sizing, positioning, focus, lifecycle, and text updates.
- Row rendering for the Windows tab and generic provider-backed tabs.
- Windows-tab filtering, display-title composition, match tiers, workspace
  biasing, and alt-tab selection handoff.
- Tab visibility, tab cycling, prefix-triggered tab/modal dispatch, and tab
  header formatting.
- Top-level key routing before feature-specific provider handlers run.
- Overlay host state, focus trapping, type dispatch, and shared confirmation UI.
- Transient UI-only visuals: selected-window ripple highlight and numbered
  workspace slot overlays.
- Transient X11 overlay drawing for ripple and slot overlays, including ARGB
  overlay windows where GTK/GDK abstractions are not sufficient.
- Direct window-activation messaging via `_NET_CURRENT_DESKTOP`,
  `_NET_ACTIVE_WINDOW`, `XMapRaised`, and `XFlush`.

### Does Not Own
- Provider data models, provider filtering, or provider action semantics.
- Generic window enumeration, EWMH state-query primitives, or reusable X11
  backend abstractions; those live in `x11/`.
- Geometry planning or durable move/resize policy; those live in `geom/` and
  feature command handlers.
- Low-level matching/ranking primitives, command execution semantics, or
  feature persistence.
- Feature-specific overlay content and key handling beyond dispatching to it.

## Public Surface
- `display.h`, `display_pipeline.h`, `dynamic_display.h`: display refresh,
  render pipeline, sizing, scrollbars, `activate_window`.
- `window_filter.h`, `window_display_title.h`: Windows-tab filtering,
  alt-tab selection, and title composition with Names display prefixes.
- `key_handler.h`: `on_key_press`, `on_entry_changed`, `handle_navigation_keys`.
- `tab_switching.h`, `tab_header.h`, `tab_metadata.h`, `prefix_tabs.h`,
  `cofi_modal.h`: tab visibility, header formatting, prefixes, and modals.
- `overlay_manager.h`, `overlay_dispatch.h`, `overlay_confirm.h`: overlay host,
  type dispatch, and confirmation entrypoints.
- `gtk_utils.h`, `gtk_window.h`, `slot_overlay.h`, `window_highlight.h`,
  `window_lifecycle.h`: widget helpers, alignment, slot overlays, ripple
  highlight, and popup lifecycle.

## Acceptance Criteria
1. `update_display` renders the active tab, appends header/candidates, clips to
   the display column budget, and writes the text buffer.
2. The Windows tab displays filtered windows bottom-up with selection markers,
   harpoon slot labels, one-based desktop labels, and fitted instance/title/class
   columns.
3. Provider tabs render through `CofiTabProvider` row-count, `format_row`,
   slot-payload, shortcut-hint, and filtered-map hooks rather than feature-owned
   data.
4. Display range calculation clamps visible rows to `[scroll_offset,
   scroll_offset + max_lines)` and never reads outside the requested item count.
5. Scrollbars appear only when total items exceed visible rows and use the
   flipped offset required by the bottom-up list layout.
6. Fixed-window sizing derives a monospace grid from realized text metrics,
   stores `fixed_cols`/`fixed_rows`, recenters, and invalidates display sizing.
7. Tab headers include only visible tabs, emphasize the active tab, and collapse
   with `<`/`>` indicators when the list exceeds display width.
8. Tab switching cycles visible tabs only; surfaced tabs clear when returning to
   a pinned tab.
9. Switching tabs calls previous provider `on_leave`, stops its tick, clears
   entry text, initializes the target, resets selection, and refreshes.
10. Prefix dispatch treats `:` as command mode, provider prefixes as modal
    providers, and tab prefixes as tab claims while preserving the origin tab.
11. Clearing entry text after a prefix claim returns to the origin tab and clears
    the active prefix claim.
12. Modal providers surface their tab, clear entry text, set the mode indicator,
    and restore the origin tab on exit.
13. Modal Escape obeys provider policy: clear non-empty text first for
    `COFI_MODAL_CLEAR_THEN_RETURN`, else exit immediately.
14. Active overlays consume all key events before any other handler. Prefix
    characters dispatch before command/modal and tab-specific handlers. Harpoon
    handlers run after provider handlers but before tab switching, repeat
    action, and shared navigation.
15. Entry changes update command candidates in command mode; otherwise they
    apply prefix claims, invoke Windows filtering or provider query callbacks,
    preserve provider selection, and refresh display.
16. Showing an overlay makes the popup visible if needed, replaces any active
    overlay, traps focus, and focuses the relevant entry or modal background.
17. Hiding an overlay removes widgets, clears overlay-specific pending state,
    restores main focusability, regrabs hotkeys after capture, and focuses entry.
18. Confirmation overlays own title/info strings, show the standard hint, run the
    callback only on confirm keys, and hide on confirm or cancel.
19. After the popup appears, live match entries are rebound, the active tab is
    initialized and displayed, window sizing reflects current monitor/font
    metrics, focus is granted to the entry field, and slot overlays render on an
    idle callback after realization.
20. After hiding, no provider/focus timers, initial-overlay idles, animations,
    or slot overlays remain active; the window resets to the Windows tab with a
    cleared entry in normal command mode, surfaced-tab and overlay state are
    cleared, and config/harpoon state is persisted.
21. Ripple highlighting is skipped when disabled, replaces any prior ripple,
    centers an ARGB popup over the target window, animates for the fixed duration,
    and cleans up the frame-clock handler.
22. Slot overlays are skipped when disabled, replace existing overlays, draw
    one-based numbers over assigned workspace-slot windows, and auto-destroy
    after the configured duration.
23. `activate_window()` switches to the target window's desktop when the
    `_NET_WM_DESKTOP` property is available, sends `_NET_ACTIVE_WINDOW`, raises
    the window with `XMapRaised`, and flushes the X11 command stream.
24. Empty Windows queries preserve current window order, expose every current
    window in `filtered`, and apply the configured alt-tab selection policy.
25. Non-empty Windows queries score against the same display string users see:
    desktop label, display instance/class, and title with any custom name prefix
    resolved from the Names store.
26. Workspace bias promotes current-workspace windows only within compatible
    score tiers; it must not let a weaker tier outrank a stronger direct match.
27. Filtering records the selected window id after sorting so later refreshes
    can preserve selection when that window is still present.
28. Display-title composition prefixes a custom name from the Names store as
    `<custom name> - <window title>` and otherwise returns the original title.

## Notes
`activate_window()` performs raw X11 desktop switching, activation messaging,
raising, and flushing from `display.c`. Keep it in sync with any future x11/
activation helper to avoid split-brain behavior.

`slot_overlay.c` and `window_highlight.c` intentionally bypass x11/ for
transient X11 overlay windows because they need ARGB visuals, direct positioning,
and short-lived rendering behavior. That duplication is deliberate but creates
drift risk if generic X11 overlay helpers are introduced.

`dynamic_display.c` currently hardcodes dynamic sizing defaults to 50% screen
height, 5 minimum lines, 50 maximum lines, and 20 fallback lines while a TODO
blocks config integration. Config changes do not affect dynamic display sizing
until that integration is implemented.

`key_handler.h` forward-declares harpoon handlers implemented in
`harpoon/key_handler_harpoon.c`. New feature-specific key-handler patterns
should follow that split: declare the UI dispatch seam here, implement feature
behavior in the feature folder.
