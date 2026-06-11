# Files

## Purpose
Files provides the `:files` tab: an async `fd`-backed file finder rooted at
`$HOME`, with a per-daemon in-memory corpus and in-process fuzzy rescoring for
each query.

## Boundary

### Owns
- Files tab registration, row rendering, refresh/open shortcuts, and `files`
  command surfacing.
- Async `fd` / `fdfind` subprocess startup, NUL-delimited stdout parsing,
  timeout cancellation, generation-guarded callback discard, and session-lifetime
  cache replacement.
- In-memory cached file corpus, top-50 filtered rows, and `xdg-open`
  dispatch on Enter.

### Does Not Own
- File content inspection, MIME-aware open policy beyond delegating to
  `xdg-open`, or recency/frequency learning.
- Shell parsing or command-string construction; subprocesses are launched only
  with argument-form `g_subprocess_new()`.
- Centralized help text for per-tab keys; shortcut hints live on the provider.

## Public Surface
- `files_provider.h`: `files_provider_register()`
- `files_search.h`: `FilesMode`, `init_files_mode()`, `cleanup_files_mode()`,
  `files_on_enter()`, `files_on_leave()`, `files_on_query_changed()`,
  `files_search_refresh()`, `files_row_count()`, `files_path_at_visible()`,
  `files_match_string()`, `files_row_identity()`, `files_status_message()`,
  `files_status_is_error()`
- Test-only hooks under `COFI_TESTING`:
  `files_search_set_spawn_impl_for_test()`,
  `files_search_set_tool_path_for_test()`,
  `files_search_has_pending_for_test()`,
  `files_search_reset_for_test()`,
  `files_path_excluded_for_test()`

## Acceptance Criteria
1. The Files provider registers as a hidden dynamic tab, exposes the `files`
   command, and shows Enter/Open plus `r`/Refresh in its provider shortcut hint.
2. The first `:files` activation starts an async `fd` corpus build rooted at
   `$HOME`; later queries reuse the cached corpus until the user presses `r`.
3. All file search subprocesses launch asynchronously through
   `g_subprocess_new()`; this subsystem never uses `_sync` subprocess APIs.
4. If `files.fd_path` is empty, the subsystem resolves `fd` first and
   `fdfind` second; if neither exists, the tab becomes unavailable with an INFO
   log and no blocking retry loop.
5. The fd subprocess always uses NUL-delimited output and the parser splits on
   `\0`, so filenames containing embedded newlines are cached as a single row.
6. The cache loader honors built-in excludes for common noise and credential
   directories, appends any `files.excludes` patterns, and never surfaces files
   under `.ssh`, `.gnupg`, `.aws`, `.docker`, or `.password-store`.
7. Each search kickoff increments the generation token; if a late callback's
   generation no longer matches the current one, it frees its resources and
   makes no cache or UI mutation.
8. Each fd subprocess has a 5000 ms watchdog; when it fires, the subprocess is
   cancelled and force-exited without replacing the current cache.
9. The cache is capped at 50,000 paths; once the cap is reached, the subprocess
   is force-exited, a warning is logged, and only the accepted prefix is kept.
10. Query filtering is in-process against the cached corpus using `fzf_has_match`
    then `fzf_fuzzy_match` on the full path, keeping only the best 50 rows for
    display.
11. When the active Files tab receives a cache replacement, it preserves
    selection by row identity before swapping the backing paths and restores
    selection after rebuilding filtered rows.
12. Pressing Enter on a file row launches `xdg-open <path>` asynchronously and
    dismisses cofi on success; pressing `r` cancels any in-flight load and
    starts a fresh async fd scan.

## Notes
- `fd`'s default ignore behavior is intentional here: `.gitignore` and
  `.fdignore` naturally suppress `target/`, `build/`, and similar churn
  without extra cofi-side rules.
