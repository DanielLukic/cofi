# Core Log

## Purpose
Core logging provides the process-wide diagnostic surface used by cofi subsystems and tests.

## Boundary

### Owns
- Process-wide log level filtering.
- Severity-tagged formatting with timestamp and `file:line` origin.
- The default stderr sink and registration of additional `FILE *` sinks.

### Does Not Own
- Log file rotation, archival, or external aggregation.
- Per-subsystem categories, namespaces, or structured log fields.
- Deciding which events other subsystems should log.

## Public Surface
- `core/log/log.h`
- `log_trace()`, `log_debug()`, `log_info()`, `log_warn()`, `log_error()`,
  `log_fatal()`
- `log_set_level()`, `log_set_quiet()`, `log_add_fp()`
- `log_level_string()`

## Acceptance Criteria
1. Every emitted log message includes a timestamp, severity level, source file,
   and source line before the formatted caller message.
2. `log_set_level()` changes the minimum severity emitted to stderr at runtime
   without requiring a rebuild or process restart.
3. `log_set_quiet(true)` suppresses the default stderr sink while preserving
   registered file sinks.
4. File sinks registered with `log_add_fp()` receive messages at or above their
   own threshold, independent of the stderr threshold.
5. Registering more than the fixed callback capacity fails with `-1` instead
   of overwriting an existing sink.
6. Logging is not thread-safe. Callers concurrently emitting from multiple
   threads must serialize externally or risk interleaved output and inconsistent
   sink iteration.
7. Severity strings remain stable for `TRACE`, `DEBUG`, `INFO`, `WARN`,
   `ERROR`, and `FATAL`.

## Notes
`log_log()` is exposed because the per-level macros expand to it. Treat it as
an internal entry point; prefer `log_info()` / `log_warn()` / etc. in caller code.

Keep this file focused on product boundaries and behavior seeds. Implementation details belong in code comments or subsystem documentation only when they affect future changes.
