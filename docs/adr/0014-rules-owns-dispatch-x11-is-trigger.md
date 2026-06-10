# 0014 — Rules subsystem owns automatic dispatch; x11 is event-trigger only

**Status:** Accepted
**Date:** 2026-06-01
**Supersedes:** none

## Context

cofi evaluates rules automatically on window appearance (`_NET_CLIENT_LIST`
change) and window title change (`PropertyNotify` on `WM_NAME` /
`_NET_WM_NAME`). Both trigger paths originate in `src/x11/x11_events.c`.

Before this decision, `x11_events.c` contained the entire dispatch loop:
window iteration, per-rule trigger gating (`rule_trigger_allows`), transition
state evaluation (`check_rule_match`), fire-once suppression, circuit-breaker
checks, re-entry guard, `RULE:` logging, and command dispatch
(`execute_command_background`). Rule semantics were expressed in the X11 event
handler, not in the rules subsystem.

This made it impossible to test rule dispatch without a real X11 event path,
required X11 edits for every rule-semantics change, and violated the intended
separation: x11 owns event detection; rules owns evaluation policy.

The refactor (`2be9ff1`, TFD-836) extracted the dispatch loop into
`src/rules/rules_dispatch.c`. `x11_events.c` was reduced to three call sites
that forward event context to the rules layer. A boundary test in
`test/test_matching_boundaries` enforces the separation by failing if
`x11_events.c` references `check_rule_match` or `rule_breaker_*` directly.

## Decision

`src/rules/rules_dispatch.c` owns the entire automatic dispatch loop: window
iteration, per-rule trigger gating, transition-state evaluation, fire-once and
`new_only` suppression, circuit-breaker enforcement, re-entry guard, logging,
and command dispatch.

`x11_events.c` is the event trigger only: it calls `rules_apply()` on
`_NET_CLIENT_LIST` changes (supplying newly-added window IDs) and
`rules_apply_for_title_change()` on per-window title changes. It does not
evaluate, gate, or execute rules itself.

The boundary is unidirectional: x11 calls into rules; rules does not call
back into x11 event infrastructure.

## Consequences

- **Rules dispatch is independently testable.** `test/test_rules_dispatch.c`
  exercises the full dispatch loop — trigger gating, new-window gate, re-entry
  guard, startup suppression — without X11, GDK, or a display connection.

- **Rule semantics changes stay in `src/rules/`.** Adding trigger types,
  adjusting gating logic, or changing fire-once behavior requires no X11
  edits.

- **Boundary is machine-enforced.** `test/test_matching_boundaries` fails the
  build if `x11_events.c` uses `check_rule_match` or `rule_breaker_*`
  directly, preventing the policy from drifting back into the event handler.

- **Future event sources compose cleanly.** A future trigger path (e.g.
  D-Bus signals, process events) can call the same `rules_apply()` entry
  points without duplicating dispatch logic.

- **`src/x11/window_list` grows one shared helper.** `window_id_in_list` was
  previously a static duplicate in both `x11_events.c` and the old dispatch
  code. It now lives in `src/x11/window_list` and is imported by both
  consumers. This is documented in `src/rules/CONTEXT.md` as an explicit
  cross-subsystem dependency.

## Alternatives considered

- **Keep the dispatch loop in `x11_events.c`.** Rejected because rule
  semantics had no natural home in x11, the loop was untestable without a live
  X11 connection, and every rule feature required edits in an unrelated file.

- **New top-level `src/dispatch/` service between x11 and rules.** Rejected
  because rules is the only current consumer of automatic dispatch. Adding an
  intermediate layer with no other clients introduced indirection without
  benefit. The dispatch entry points live in the subsystem that owns the
  policy (`src/rules/`), not in a generic intermediary.
