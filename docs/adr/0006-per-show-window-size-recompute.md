# 0006 — Per-show window-size recompute on cursor monitor

**Status:** Accepted
**Date:** 2026-05-11 (`38bad7f`)
**Supersedes:** [0005](0005-one-shot-fixed-window-size.md)

## Context

ADR-0005's one-shot init froze window geometry from the boot-time XSettings race. Symptom: wrong DPI/scaling on first show after boot; correct only after hide+show because the original cached metrics were never invalidated.

## Decision

- `show_window` invalidates `fixed_cols` / `fixed_rows` to zero and calls `init_fixed_window_size` on every show.
- Monitor selection uses `gdk_display_get_monitor_at_point()` with the current cursor position, not the primary monitor.
- After resize, recenter on the chosen monitor using `ScreenInfo.x` / `.y` and `gtk_window_move()`.

## Consequences

- No timing dependence on `mate-settings-daemon` — wrong metrics get replaced on the next show.
- Survives mid-session `xrandr` and DPI changes.
- Per-show Pango metric sampling is cheap (~sub-ms); not measurable in latency budget.
- Cross-cutting invariant added to [architecture.md](../architecture.md): *cache invalidation on show*.

## Alternatives considered

- Wrap the systemd service with `systemctl --user import-environment` and a `.desktop` autostart to defer cofi boot until XSettings is ready: red herring — the root cause was cofi caching, not env propagation.
- `WantedBy=graphical-session.target` instead of `default.target`: inactive on MATE; unreliable.
- Invalidate cache on `XSettings` PropertyNotify: more complex, no win over per-show recompute.
