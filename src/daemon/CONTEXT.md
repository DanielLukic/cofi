# Daemon

## Purpose
The daemon subsystem keeps cofi resident, accepts later invocations over a Unix
socket, owns global hotkey grabs, edits hotkey bindings, and launches detached
child processes without blocking the UI.

## Boundary

### Owns
- Unix-socket delegation from second `cofi` invocations to the resident daemon.
- Socket path derivation, listener binding, stale socket cleanup, opcode/tab payload transfer, GLib monitoring, and teardown.
- X11 global hotkey grabs, Lock/NumLock variants, retry/exit conflict UI, grab-state population, cleanup, and regrab.
- Runtime hotkey dispatch into command-mode prefill or auto-executed command
  strings.
- The Hotkeys provider tab, hotkey filtering, hotkey add/rebind/edit overlays,
  and hotkeys JSON persistence.
- Detached process launching for run/apps/projects/profiles callers, including systemd-run, fork+setsid fallback, terminals, and Exec field stripping.

### Does Not Own
- The meaning of command strings invoked by hotkeys; command parsing,
  availability, and handlers live in `commands/` and feature folders.
- Provider-specific tab behavior reached through daemon opcodes or show-mode hotkeys.
- General UI rendering, selection policy, modal hosting, or window lifecycle
  behavior outside the hotkey overlays.
- X11 window enumeration, active-window lookup semantics, workspace movement, or EWMH primitives beyond grab/event APIs.

## Public Surface
- Socket protocol and dispatch: `COFI_OPCODE_*`, `daemon_socket_*()`,
  `daemon_socket_dispatch_opcode()`, and `daemon_socket_dispatch_show_tab()`.
- Hotkey config: `HotkeyConfig`, `HotkeyBinding`, load/save, binding helpers, display formatting, and `parse_hotkey_command()`.
- Hotkey runtime: `HotkeyGrabState`, `setup_hotkeys()`, `cleanup_hotkeys()`, `regrab_hotkeys()`, `handle_hotkey_event()`, and `dispatch_hotkey_mode()`.
- Hotkeys tab/overlays: provider registration, filtering/selection helpers, overlay constructors, and overlay key handlers.
- Detached launching: `detach_launch_*()` and `detach_strip_field_codes()`.

## Acceptance Criteria
1. Socket opcodes accept only defined nonzero values; reserved or unknown bytes
   are rejected before dispatch.
2. Socket paths use `$XDG_RUNTIME_DIR/cofi.sock` when available, otherwise `/tmp/cofi.sock`, and listener binding removes only stale paths.
3. A second `cofi` process delegates an opcode or `show-tab` payload to the resident daemon and exits instead of starting another instance.
4. The daemon monitor reads queued socket messages without blocking GTK and
   keeps the watcher alive across transient accept/read failures.
5. Delegated opcodes refresh focus timestamp metadata, reset command/modal state, show cofi, and surface only enabled provider-backed tabs.
6. Disabled provider delegate opcodes fail closed: no window show, tab switch,
   modal entry, or command-state mutation.
7. The command delegate captures the active X11 window before showing cofi; the run delegate enters the Run modal only when the run provider is enabled.
8. `show-tab` delegation resets interaction modes, shows cofi, invokes the
   provider command surface by name, and returns focus to the entry.
9. Default hotkeys are Alt+Tab for Windows, Alt+grave for Command, and
    Alt+BackSpace for Workspaces, each as an auto-executing command.
10. Hotkey loading skips malformed bindings, preserves valid empty arrays as empty configs, and leaves default-bootstrap policy to callers.
11. Hotkey saves write the canonical `{"hotkeys":[...]}` JSON shape through the
    shared JSON persistence helper.
12. Adding a binding updates an existing key in place or appends until
    `MAX_HOTKEY_BINDINGS`; removing compacts the remaining bindings in order.
13. `parse_hotkey_command()` classifies empty/`list` as display, key-only or
    `del`/`rm`/`remove` as unbind, and key plus text as bind.
14. Hotkey grab state includes only bindings whose shortcuts parse into
    supported X11 key/modifier combinations.
15. `setup_hotkeys()` grabs every parsed binding for base, Lock, NumLock, and Lock+NumLock states; failures ungrab partial registrations and retry only if the user chooses Retry.
16. X11 hotkey matching ignores Lock/NumLock bits, requires exact remaining
    modifiers and keycode, and schedules dispatch on GTK idle.
17. Commands ending in `!` auto-execute against the active non-cofi window; commands without `!` prefill command mode for user editing.
18. Auto-executed hotkeys show cofi only when command metadata keeps the UI
    open; otherwise they hide any visible cofi window after dispatch.
19. The Hotkeys tab filters by key and command, shows an empty-state row when no
    bindings match, and identifies rows as `hotkey:<key>`.
20. Ctrl+A/Ctrl+B stop active grabs and open add/rebind capture; Ctrl+D deletes, saves, regrabs, refilters, clamps, and refreshes; Ctrl+E edits a selected command.
21. The add/rebind overlay canonicalizes typed or captured shortcuts, rejects duplicate adds, asks Y/N before replacing an existing binding, and keeps validation errors visible.
22. Detached launch APIs reject empty commands, prefer `systemd-run --user --scope`, fall back to fork+setsid with the correct stdio policy, and report exec failure.
23. Terminal launches resolve `$TERMINAL`, desktop terminals, then known binaries; Exec field stripping drops `%X`, keeps `%%`, and trims trailing spaces.

## Notes
- Socket and hotkey paths choose what surface to invoke; providers and command handlers still define behavior.
- Hotkey add/rebind temporarily calls `cleanup_hotkeys()` so the overlay can capture the key.
- `detach_launch.*` is shared process infrastructure for run, apps, projects, and profiles.
- `COFI_DISABLE_SYSTEMD_RUN=1` forces detached launches down the fork+setsid
  fallback path, used by integration tests whose fake terminal binaries live on
  the harness `PATH`.
