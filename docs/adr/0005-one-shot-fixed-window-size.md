# 0005 — One-shot fixed-window-size initialization

**Status:** Superseded by [0006](0006-per-show-window-size-recompute.md)
**Date:** 2026-04-06 (`0d6b528`)

## Context

cofi's main window uses a precomputed width × height derived from Pango font metrics (monospace cell × cols/rows) so the row count is stable and layout doesn't reflow per keystroke. Initial implementation sampled metrics once at startup, cached them, and used them for every subsequent show.

## Decision

`init_fixed_window_size` called once during app init. Authority over `fixed_cols` / `fixed_rows` for the process lifetime.

## Consequences

Window dimensions effectively locked to whatever XSettings / monitor state existed at the first show.

## Why it broke

Systemd starts cofi early (`WantedBy=default.target`), racing `mate-settings-daemon`. The first show on first boot caught wrong DPI / scaling / font metrics — frozen for the entire session. `xrandr` changes mid-session also did not update the layout.
