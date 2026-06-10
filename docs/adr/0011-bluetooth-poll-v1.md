# 0011 — Bluetooth tab v1 polling and async D-Bus policy

**Status:** Accepted
**Date:** 2026-06-10
**Supersedes:** none

## Context

The Bluetooth tab adds a new external-data provider backed by BlueZ on the
system D-Bus. Unlike the X11 window list, Bluetooth state lives outside cofi's
existing event-driven property-notify path. The first design round considered
three extra mechanisms on top of the core provider flow:

- a daemon-lifetime `PropertiesChanged` subscription for push updates
- sync GDBus helpers for simpler call sites
- a watchdog timer layered on top of the D-Bus per-call timeout

All three conflicted with the core product constraint for this subsystem:
**cofi must never block or hang on Bluetooth work** while keeping v1 as simple
as possible to ship and maintain.

## Decision

Bluetooth v1 uses provider `on_tick` polling at a 3-second cadence rather than
`PropertiesChanged` subscription updates.

Every D-Bus call in `src/bluetooth/` uses async `g_dbus_connection_call()` with
an explicit `timeout_msec`. Sync GDBus helpers are forbidden in this subsystem.

The additional watchdog timer proposed during review is rejected. Per-call
D-Bus timeouts are the only timeout mechanism.

## Consequences

- **Polling over subscription.** v1 refreshes the active Bluetooth tab by
  reissuing `GetManagedObjects` on a 3-second `on_tick`. This avoids
  daemon-lifetime signal subscription state, property-map filtering logic,
  unsubscribe-on-shutdown code, and another callback path mutating the model.
  The trade-off is an acceptable up-to-3-second lag before passive state
  changes appear.

- **Async-only D-Bus mandate.** `src/bluetooth/` uses
  `g_dbus_connection_call()` for `GetManagedObjects`, `Connect`, and
  `Disconnect`, each with an explicit timeout. `_sync` helpers are forbidden so
  the GTK main loop is never blocked by Bluetooth operations.

- **No watchdog timer.** GDBus async calls already complete exactly once with
  success, timeout, or cancellation. Adding a `g_timeout_add()` watchdog beyond
  the D-Bus timeout does not protect against a credible extra failure mode, and
  introduces double-free / use-after-free risk if it races the normal callback.
  The subsystem relies on the D-Bus timeout itself.

- **Upgrade path stays open.** If the 3-second lag proves too slow in practice,
  v2 can replace the polling cadence with `PropertiesChanged` without changing
  the provider/tab surface or the async-only D-Bus contract.

## Alternatives considered

- **`PropertiesChanged` subscription in v1.** Rejected for v1 complexity.
  It would require daemon-lifetime subscription ownership, sender/interface
  filtering, property-map parsing in the signal path, and shutdown unsubscribe
  coordination for a feature that can tolerate a small refresh lag.

- **Sync GDBus calls.** Rejected because they can block the GTK main loop and
  violate the explicit "cofi must never block on Bluetooth" requirement.

- **Timeout watchdog layered over async D-Bus.** Rejected because the callback
  already resolves exactly once on success, timeout, or cancellation. The extra
  watchdog protects against no real failure mode while making ownership and
  callback ordering harder to reason about.
