# Architecture Decision Records

Decisions that had real alternatives and evolved over time. Tiny by design.

Format: numbered, dated, with `Status` + `Supersedes` / `Superseded by` headers.

## Index

| # | Title | Status |
|---|---|---|
| 0001 | [D-Bus single-instance + spawn-per-invocation](0001-dbus-single-instance.md) | Superseded by 0002 |
| 0002 | [Single-binary daemon with Unix-socket opcode delegation](0002-unix-socket-opcode-daemon.md) | **Accepted** |
| 0003 | [Multi-stage fzy scoring](0003-multistage-fzy-scoring.md) | Superseded by 0004 |
| 0004 | [Single-pass fzf FuzzyMatchV2 over the full display row](0004-fzf-fuzzymatchv2-full-row.md) | **Accepted** |
| 0005 | [One-shot fixed-window-size init](0005-one-shot-fixed-window-size.md) | Superseded by 0006 |
| 0006 | [Per-show window-size recompute on cursor monitor](0006-per-show-window-size-recompute.md) | **Accepted** |
| 0007 | [Registry-based tab architecture (CofiTabProvider)](0007-provider-registry-architecture.md) | **Accepted** |
| 0008 | [Cluster-correct UTF-8 column rendering](0008-utf8-column-rendering.md) | **Accepted** |
| 0009 | [Tolerant JSON I/O via `cofi_json_io`](0009-tolerant-json-io-via-json-glib.md) | **Accepted** |
| 0010 | [Shared confirm-overlay primitive (`show_confirm_overlay`)](0010-shared-confirm-overlay-primitive.md) | **Accepted** |

## Topics intentionally not recorded

These were never alternatives — pure day-one choices with no evolution. Captured here so future readers don't waste time digging:

- **Xlib + EWMH directly** (vs `wmctrl` / `libwnck` / `xdotool`) — Xlib from the first C commit; `wmctrl` references in early code comments referred to the Go predecessor `gofi`.
- **GTK3** — chosen at the C port; never evaluated against GTK4 / Qt / ImGui.
- **Event-driven window list** (PropertyNotify on root via `GIOChannel`) — present in the first commit; cofi never polled.
- **Test strategy** (70 small standalone binaries, hand-stubbed cross-cutting deps) — pattern set in the second test ever; no framework (cmocka/check/criterion) considered.

## Template

```markdown
# NNNN — Title

**Status:** Draft | Accepted | Superseded by NNNN
**Date:** YYYY-MM-DD
**Supersedes:** NNNN  (omit if first)

## Context
What forces, what constraints, what came before.

## Decision
What we are going to do — concrete and specific.

## Consequences
What becomes easier or harder; what we accept.

## Alternatives considered
Other options and why they were rejected.
```

## See also

- [docs/decisions/](../decisions/) — design exploration notes for the plugin API (not strict ADRs). The architecture they explored is now recorded in [0007](0007-provider-registry-architecture.md); the decisions docs remain as historical context.
