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
- Sessions is a provider tab with heavier query handling: the left side of
  `terms | refine` starts a live cancellable corpus search, while the right side
  refines already-grouped session rows.
- Leading `:` pasted into the entry enters Command mode and strips the leading
  `:` from the command text.
- Leading non-`:` prefixes can claim/surface their target tab from entry-change
  paths where supported, and the claim is released when the entry returns to
  empty.
- Command and modal modes own entry changes while active; Normal-mode filtering
  must not run underneath them.

## Tab Visibility Model

Tabs have three visibility states that control whether they appear in Tab/Shift+Tab cycling:

- **PINNED** — always visible and always reachable via Tab/Shift+Tab: Windows, Apps.
- **SURFACED** — becomes Tab-reachable after being opened by a `:show <verb>` command, a prefix key, or a delegate flag. Returns to HIDDEN when cofi next hides.
- **HIDDEN** — not reachable via Tab/Shift+Tab until surfaced. All tabs except Windows and Apps start HIDDEN.

Tab/Shift+Tab cycles only PINNED and currently-SURFACED tabs. HIDDEN tabs are reachable only through their command or prefix.

## Hotkey Auto-Execute Marker (`!`)

Hotkey command strings accept a trailing `!` to control dispatch:

- **With `!`** — the command executes immediately. If it requires UI (e.g. a tiling overlay), cofi opens automatically. If it does not (e.g. `:jw 3`), it runs silently against the active X11 window with no window shown.
- **Without `!`** — cofi opens with the command pre-filled in the command entry, letting the user review or add arguments before pressing Enter.

The `!` marker is meaningful only in hotkey bindings stored in `hotkeys.json`. It has no effect when commands are typed interactively.

## Modal Provider Policies

Modal providers declare an exit policy that controls what happens when the user presses Escape:

- **`COFI_MODAL_HIDE_ON_ESC`** — Escape exits the modal and returns to the origin tab without altering the entry text. Used by most provider tabs (Harpoon, Names, Sessions, Projects, etc.).
- **`COFI_MODAL_CLEAR_THEN_RETURN`** — Escape first clears the entry when it is non-empty; a second Escape exits the modal. Used by Run (`!`), where the entry holds live command text the user may want to clear before dismissing.
- **`COFI_MODAL_RETURN_KEEP_QUERY`** — Escape exits the modal and restores whatever query was in the entry before the modal was entered. Used when the modal is a transient overlay that should not discard the user's filter state.
