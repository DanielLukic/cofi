# Modes

This document specifies mode switching behavior. It is the contract for key
handling, prefix dispatch, and entry-change routing.

## States

- **Normal mode** filters the active tab and uses `>` as the indicator.
- **Command mode** is entered by `:` and uses `:` as the indicator. It runs
  window/app commands against the current selected target.
- **Modal provider mode** is entered by a provider prefix such as `!` or `=`.
  The provider owns Enter/Escape/query behavior until it exits.

## Prefix Keys

Prefix keys are single-character mode or tab claims:

- `:` enters Command mode.
- `!` enters Run modal.
- `=` enters Calc modal.
- `$` surfaces Apps in PATH mode.
- `\` surfaces Apps in default mode.
- `>` surfaces Windows.

Most prefix ownership is registry-backed:

- `!` and `=` are modal provider prefixes resolved through the provider registry.
  Disabled providers make these prefixes fail closed.
- `$` and `\` are Apps tab-claim prefixes resolved through the provider registry.
- `>` remains a core tab-claim prefix for Windows.

## The Special `:` Rule

`:` is an escape hatch from filtering into Command mode.

- In Normal mode, pressing `:` always enters Command mode, even when the entry
  already contains filter text.
- The existing filtered list and selected row must remain the command target
  context. The `:` key must be consumed; it must not be appended to the filter.
- This lets a user filter to a specific window, press `:`, then run a command on
  that selected window.

This behavior is intentionally different from other prefixes.

## Other Prefix Rules

- Non-`:` prefixes dispatch from Normal mode only when the entry is empty.
- Non-`:` prefixes with a non-empty filter are ordinary query text unless the
  active mode already owns them.
- From Command mode, pressing a different prefix on an empty entry exits Command
  mode and dispatches that prefix.
- From Modal provider mode, pressing a different prefix on an empty entry exits
  the current modal and dispatches that prefix.
- Pressing the same active prefix again is not re-dispatched; the active mode
  handles it normally.

## Entry-Change Routing

- Normal mode entry changes filter the active tab.
- Agent Sessions is a provider tab with heavier query handling: the left side of
  `terms | refine` starts a live cancellable corpus search, while the right side
  refines already-grouped session rows.
- Leading `:` pasted into the entry enters Command mode and strips the leading
  `:` from the command text.
- Leading non-`:` prefixes can claim/surface their target tab from entry-change
  paths where supported, and the claim is released when the entry returns to
  empty.
- Command and modal modes own entry changes while active; Normal-mode filtering
  must not run underneath them.
