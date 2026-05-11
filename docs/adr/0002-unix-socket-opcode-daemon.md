# 0002 — Single-binary daemon with Unix-socket opcode delegation

**Status:** Accepted
**Date:** 2026-04-18 (`ed73292`, TFD-530)
**Supersedes:** [0001](0001-dbus-single-instance.md)

## Context

D-Bus IPC + spawn-per-invocation (ADR-0001) was too slow on keypress paths. Intermediate steps: `8c1495a` (2026-03-27) introduced XGrabKey-based hotkeys with informal single-instance enforcement; `d3bb9cc` (2026-03-27) started the daemon hidden at boot; `7ee83b0` (2026-03-27) removed D-Bus, leaving instance coordination ad-hoc.

## Decision

One binary, two roles:

- **First invocation** binds `$XDG_RUNTIME_DIR/cofi.sock` (fallback `/tmp/cofi.sock`), grabs global hotkeys, keeps the GTK toplevel hidden, runs the X11 event loop.
- **Second invocation** finds the socket bound, sends a one-byte **opcode** (1=windows, 2=workspaces, 3=harpoon, 4=names, 5=command, 6=run, 7=applications; byte 0 reserved for protocol version) plus argv tail, and exits.

Stale-socket recovery on bind. `--no-daemon` flag removed.

## Consequences

- Keypress latency dominated by GTK show, not IPC connect.
- The daemon is the canonical source of state; delegators carry no state.
- A crashing daemon leaves a stale socket — recovery is on bind, not connect.
- TFD-511 active-window-capture fix could route through the socket path cleanly.

## Alternatives considered

- Two binaries (`cofid` + `cofi-client`): rejected — install/version skew, no win for users.
- Abstract Unix socket: portability not required; pathname socket allows simple stat/unlink.
- Re-using D-Bus: brought no value once we own a hotkey-grabbing daemon.
