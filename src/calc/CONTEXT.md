# Calculator

## Purpose
Calculator evaluates inline arithmetic expressions and exposes the calculator modal/provider surface.

## Boundary

### Owns
- Arithmetic expression preparation, evaluation, result formatting, and error reporting for the calculator mode.
- Calculator history ordering, capacity, `last_result` tracking, and JSON persistence.
- The dynamic Calc tab provider, including row formatting, key handling, modal entry, and command registration.
- Clipboard copy behavior for successful calculator evaluations from the Calc tab.
- The `calc` command and `=` prefix modal surface.

### Does Not Own
- Generic command parsing, provider registry behavior, or modal tab infrastructure.
- The tinyexpr evaluator implementation beyond preparing inputs and interpreting its success/error result.
- Clipboard backend mechanics beyond setting the evaluated result text.
- Process startup ordering, except that app initialization calls `calc_history_load()`.
- Global selection movement, display rendering, or command-mode dispatch outside the Calc provider callbacks.

## Public Surface
- `calc/calc.h`
- `CalcEntry`, `CalcMode`
- `CALC_HISTORY_CAP`, `CALC_RESULT_LEN`, `CALC_EXPR_LEN`
- `calc_eval()`, `calc_push()`, `calc_format_double()`
- `calc_prepare_expr()`, `calc_clear()`
- `calc_history_save()`, `calc_history_load()`
- `calc/calc_provider.h`
- `calc_provider_register()`

## Acceptance Criteria
1. The Calc tab is optional and hidden by default. It accepts the `=` prefix,
   clears input on enter/exit through the clear-then-return modal policy, and
   immediately evaluates command arguments passed via `:calc <expr>` or
   `:ca <expr>`.
2. Registering the provider also registers the `calc` command and `ca` alias;
   invoking the command exits command mode, records the current tab as prefix
   origin, claims the `=` prefix, enters the Calc modal, and evaluates
   non-empty command arguments immediately.
3. Entering the Calc tab changes the entry placeholder to `expression`.
4. Calculator expressions strip leading spaces, one optional leading `=`, and
   digit separators written as underscores between digits before evaluation.
5. If a prepared expression starts with an operator and `last_result` is set,
   evaluation prepends `last_result` so chained inputs like `*2` or `=+5`
   continue from the previous successful result.
6. Successful evaluations use the C numeric locale regardless of the process
   locale, format lossless integers without a decimal point, and format
   non-integers with trimmed `%g`-style output.
7. Decimal commas are rejected as bad expressions, while commas inside
   function argument lists such as `pow(2,3)` and `atan2(0,1)` are accepted.
8. Failed evaluations return `FALSE`, write `[error: bad expression]` to the
   result buffer, push the failed expression to history, and leave
   `last_result` unchanged.
9. Empty prepared expressions return `FALSE` without mutating history.
10. Every non-empty evaluation attempt, successful or failed, is inserted at
    the front of history and persisted immediately.
11. History keeps newest entries first, is capped at `CALC_HISTORY_CAP`, and
    drops the oldest entries when capacity is exceeded.
12. Successful pushes update `last_result`; error pushes preserve the prior
    `last_result` so later operator-continuation inputs still use the last
    numeric result.
13. Calculator history persists to
    `~/.config/cofi/calc_history.json` with `entries` and `last_result`,
    and the config directories are created on first access, whether that
    access is a load or save.
14. Loading history clears the in-memory calculator first, ignores missing or
    unreadable history files, caps loaded entries at `CALC_HISTORY_CAP`, and
    restores `last_result` for operator continuation after restart.
15. Calc rows display two cells: the result in a fixed-width first column and
    the expression prefixed with `= ` in a flexible second column.
16. Row matching and row identity are both the stored expression string.
17. Pressing Enter in the Calc tab evaluates the current entry widget text.
    Pressing Ctrl+C evaluates `app->command_mode.command_buffer` instead
    (see TFD-832); successful evaluations copy the resulting `last_result` to
    the clipboard and keep cofi open.
18. Pressing Enter or Ctrl+C with empty entry text is a no-op.
19. Pressing Ctrl+X in the Calc tab clears history and `last_result`, persists
    the empty history, refreshes display, and consumes the key event.

## Notes
The calculator intentionally supports only `.` as the decimal separator, even under locales that use `,`. The `=` prefix is a modal entry mechanism, not part of the expression stored in history.

`calc_history_path()` creates config directories as a side effect on every
call. Both load and save paths trigger `mkdir`; there is no separate
calculator-history initialization step.

Ctrl+C in the Calc tab evaluates `command_mode.command_buffer`, not the entry
widget text. This diverges from Enter and is tracked as TFD-832.
