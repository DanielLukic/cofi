# Core Nav Keys

## Purpose
Core navigation keys normalize keyboard input into cofi row-navigation intent.

## Boundary

### Owns
- Translating raw GDK key press events into `NAV_UP`, `NAV_DOWN`, or `NAV_NONE`.
- Defining the shared navigation direction enum used by list and help surfaces.
- Keeping arrow-key and Ctrl+j/k navigation aliases consistent across callers.
- Returning a non-navigation result for keys this subsystem does not own.

### Does Not Own
- Moving selection indices, scrolling help pages, or refreshing the display.
- Handling Enter, Escape, Tab switching, provider shortcuts, or command execution.
- Deciding whether a caller consumes or propagates a `NAV_NONE` key event.
- Configurable keybinding storage or user-facing shortcut editing.

## Public Surface
- `core/nav_keys/nav_keys.h`
- `NavDirection`
- `NAV_NONE`, `NAV_UP`, `NAV_DOWN`
- `nav_direction_from_key()`

## Acceptance Criteria
1. `nav_direction_from_key(NULL)` returns `NAV_NONE` so callers can safely
   treat missing events as unhandled navigation.
2. `GDK_KEY_Up` maps to `NAV_UP` and `GDK_KEY_Down` maps to `NAV_DOWN`
   without requiring modifier keys.
3. `Ctrl+k` maps to `NAV_UP` and `Ctrl+j` maps to `NAV_DOWN`, matching the
   keyboard aliases used by command mode, help mode, and normal list handling.
4. Bare `j` and bare `k` return `NAV_NONE` so normal text entry can still use
   those characters.
5. Ctrl+j/k mappings require the Control mask to be present, but tolerate
   additional modifiers such as Shift.
6. Unrecognized key values return `NAV_NONE` and leave the caller responsible
   for deciding whether the event falls through to other handlers.
7. The subsystem only classifies navigation direction; callers perform the
   resulting selection movement, help scrolling, display refresh, or event
   consumption.

## Notes
The current API is intentionally not user-configurable. Add configurable navigation bindings in a higher-level input or settings subsystem rather than expanding this helper into a storage layer.
