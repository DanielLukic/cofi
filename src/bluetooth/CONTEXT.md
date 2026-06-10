# Bluetooth

## Purpose
Bluetooth provides a provider-backed tab for paired BlueZ devices so users can
inspect connection state and trigger connect or disconnect actions without
blocking cofi.

## Boundary

### Owns
- Bluetooth tab registration, command aliases, row rendering, query filtering,
  shortcuts, and tick-driven refresh behavior.
- Daemon-scoped Bluetooth state: powered adapters, paired devices, filtered
  rows, transient row state, dirty-display state, refresh generation, and
  per-device operation contexts.
- Async BlueZ D-Bus calls for bus acquisition, `GetManagedObjects`, and
  per-device `Connect` / `Disconnect`, including reply parsing and bounded
  per-call timeouts.

### Does Not Own
- Provider registry, generic tab rendering, selection primitives, or the global
  command/help systems outside this provider's registration metadata.
- Bluetooth pairing, scanning, unpairing, adapter power management, or any
  policy outside paired-device enumeration and connect/disconnect toggles.
- X11 activation, subprocess-based bluetoothctl workflows, or blocking D-Bus
  helpers from unrelated subsystems.

## Public Surface
- `bluetooth_provider.h`: `bluetooth_provider_register()`
- `bluetooth_model.h`: `BluetoothMode`, parsed adapter/device snapshot types,
  op-context types, mode init/cleanup, provider-facing row/query/action APIs,
  and BlueZ callback entrypoints.
- `bluetooth_bluez.h`: `BusState`, `bluetooth_bluez_bus_state()`,
  `bluetooth_bluez_ensure_bus()`, `bluetooth_bluez_request_refresh()`,
  `bluetooth_bluez_request_device_op()`, `bluetooth_bluez_shutdown()`

## Acceptance Criteria
1. Registering Bluetooth creates an optional hidden dynamic `BLUETOOTH` tab
   with hide-on-esc modal policy, a 3000 ms provider tick, `bluetooth` / `bt`
   command aliases, and shortcut hints only inside the provider.
2. Entering the tab never blocks: it re-renders cached dirty state if needed,
   then either starts async system-bus acquisition or issues one async
   `GetManagedObjects` refresh when the bus is already ready.
3. Bus acquisition moves through `BUS_UNINITIALIZED`, `BUS_ACQUIRING`,
   `BUS_READY`, and `BUS_FAILED`; while the bus is acquiring, repeated tab
   activations do not issue a second `g_bus_get()` request, and a failed
   acquisition leaves the tab usable with a `BlueZ unavailable` status row and
   no retry loop.
4. All BlueZ calls use async `g_dbus_connection_call()` with explicit timeouts:
   5000 ms for `GetManagedObjects`, 15000 ms for `Connect`, and 3000 ms for
   `Disconnect`.
5. `GetManagedObjects` parsing keeps only powered `org.bluez.Adapter1`
   adapters and paired `org.bluez.Device1` devices whose `Adapter` points at a
   powered adapter; the model never stores raw `GVariant` trees.
6. Empty query lists devices in deterministic order sorted by alias ascending
   and tiebroken by path; non-empty query fuzzy-matches `<alias> <adapter-name>`
   with the shared fzf scorer and keeps only matching rows.
7. Row rendering shows a persistent connected-state glyph as the first column
   (`🟢` for connected, `⚪` for not connected), then the device icon glyph,
   alias, optional adapter name (only when more than one powered adapter
   exists), and transient state text; real device rows are actionable while
   status rows are not.
8. Pressing Enter toggles connect/disconnect, `c` forces connect, `d` forces
   disconnect, and `r` starts an immediate async refresh; none of these actions
   hide cofi or block the GTK main loop.
9. Each device owns at most one active op context. Starting a new op for the
   same device cancels and replaces the previous one, bumps the generation, and
   ignores any later callback from the older generation.
10. Snapshot refresh preserves provider-row selection by calling
    `preserve_selection()` before replacing `mode->devices` or resetting
    `mode->device_count`, then calls `restore_selection()` after rebuilding,
    sorting, and refiltering the device list.
11. Op contexts are ref-counted because D-Bus callbacks may outlive op
    replacement or cancellation; a stale-generation callback must drop its ref
    and clean up without mutating the model.
12. Successful connect or disconnect callbacks update the cached connected flag,
    surface `[connected]` or `[disconnected]`, and eventually decay back to the
    idle blank state on a later redraw; failed callbacks surface `[failed]` and
    log the BlueZ error name plus message at WARN level.
13. Async callbacks update the model and set the dirty flag. They call
    `update_display()` only when cofi is visible on the Bluetooth tab; hidden
    updates stay cached until the next tab activation.
14. Leaving the tab cancels only the in-flight `GetManagedObjects` request.
    In-flight connect or disconnect operations continue in the daemon-scoped
    model and apply their result later if their generation is still current.
15. Shutdown cancels any outstanding refresh and per-device operations, then
    drops the cached D-Bus connection without waiting synchronously.

## Notes
- v1 is intentionally poll-based. There is no `PropertiesChanged`
  subscription; tab freshness comes from the 3-second provider tick plus manual
  refresh.
- This folder must not introduce `_sync` D-Bus helpers or subprocess fallbacks
  to `bluetoothctl`.
