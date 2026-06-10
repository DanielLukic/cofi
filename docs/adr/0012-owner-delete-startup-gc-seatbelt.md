# 0012 — Match-entry lifecycle: owner-delete with startup-only GC seatbelt

**Status:** Accepted
**Date:** 2026-05-29
**Supersedes:** none

## Context

Match entries (`matching.json`) are persistent identity records keyed by a
stable `match_id`. Every persisted subsystem record — harpoon slots, rules,
geom layouts, custom names — references a `match_id` to rebind its window
identity across restarts.

Before this decision, `matching_run_gc` was called at six per-action call
sites (names delete, harpoon delete/unassign/reassign, geom layout clear,
geometry clear) to reclaim orphaned entries. This created a distributed,
implicit cleanup dependency: every owner had to know to call the GC, and the
GC had to traverse all owner stores to decide what was live. The pattern was
fragile, leaked entries on crashes, and made the GC an operational dependency
rather than a recovery mechanism.

The change (`94e4718`) removed all six per-action GC call sites and replaced
them with direct owner-initiated deletion via the new
`match_entry_delete_by_match_id` primitive, which is idempotent (missing IDs
are a no-op). `matching_run_gc` was restricted to startup only.

## Decision

Each subsystem that creates a match entry is solely responsible for deleting
it when its own record is removed. Owner-delete is the only runtime deletion
path. `matching_run_gc` runs once at process startup as a crash-recovery and
legacy-data seatbelt; it is not called at runtime.

## Consequences

- **Clear ownership boundary.** Each subsystem's `CONTEXT.md` states "owns
  its match entry lifetime." No implicit coordination between subsystems is
  needed for cleanup.

- **Crash recovery on next launch.** If an owner crashes or a bug leaks an
  entry, the startup GC catches orphans at the next process start. The window
  of exposure is bounded to entries leaked since the last clean start.

- **Future owners must implement teardown.** Any new subsystem that creates
  match entries must include an explicit delete-on-removal path. Relying on
  the GC to catch omitted deletes at runtime is not acceptable — the GC does
  not run between startup and shutdown.

- **No reference counting needed.** 1:1 ownership (see ADR 0013) means an
  entry has exactly one owner at all times; no counter is required.

## Alternatives considered

- **Per-action GC scan.** Rejected because it introduced latency at every
  delete operation, required traversal of all owner stores on each call, and
  created races between the GC run and in-flight operations that still held
  references to entries being collected.

- **Reference counting on `match_id`.** Rejected because it distributed
  bookkeeping across every subsystem that touches an entry, made ownership
  non-obvious, and provided no advantage once 1:1 ownership (ADR 0013) made
  counters always `0` or `1`.

- **Central registry owning entry lifetime.** Rejected because it coupled all
  owner subsystems to a single registry module, requiring every delete to go
  through a central coordinator rather than the owner itself.
