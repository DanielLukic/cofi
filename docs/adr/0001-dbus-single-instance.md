# 0001 — D-Bus single-instance + spawn-per-invocation

**Status:** Superseded by [0002](0002-unix-socket-opcode-daemon.md)
**Date:** 2025-07-07 (introduced `c057ce0`)

## Context

Original cofi was spawn-per-invocation: every keypress launched a fresh `cofi` process, opened GTK, drew the switcher, exited. Single-instance enforcement was external (none initially); brief signal-based IPC era prior to `c057ce0` was unreliable.

## Decision

Use D-Bus to coordinate a single live instance — second invocations send a D-Bus message to the running one to bring up the switcher.

## Consequences

Acceptable correctness; startup cost dominated by D-Bus session-bus connect — measurable lag on every keypress. Later optimized with a lock-file pre-check + 150 ms timeout (`5162a29`, 2025-07-08), which papered over but did not solve the latency.

## Alternatives considered

- Plain signals (the regime before this ADR): brittle, lost on slow startups.
- File-lock only: no IPC channel, only mutex.
