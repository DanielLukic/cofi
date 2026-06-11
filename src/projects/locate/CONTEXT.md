# Projects Locate

## Purpose
Projects Locate provides the async `plocate` / `locate` fallback used by the
Projects tab when parent Projects logic requests a filesystem folder fallback.

## Boundary

### Owns
- Locate tool resolution (`plocate` first, then `locate`) and disable-if-missing
  behavior.
- Async subprocess startup, stdout streaming, timeout cancellation, cap
  enforcement, and generation-guarded callback discard.
- Query-to-glob conversion for plocate basename searches, exclude-pattern
  expansion, per-line path filtering, and post-read fuzzy rescoring of
  accepted paths.

### Does Not Own
- Projects-tab trigger policy, selection policy, row rendering, or
  `ProjectFolder` model mutation.
- tmux, zellij, zoxide, remote-scope discovery, or folder-open behavior.
- Shell parsing or command-string construction; the locate subprocess is always
  launched with argument-form `g_subprocess_new()`.

## Public Surface
- `projects_locate.h`: `ProjectsLocateResult`,
  `ProjectsLocateResultsCallback`, `projects_locate_set_results_callback()`,
  `projects_locate_search_async()`, `projects_locate_cancel_pending()`
- Test-only hooks under `COFI_TESTING`:
  `projects_locate_glob_for_test()`,
  `projects_locate_path_excluded_for_test()`,
  `projects_locate_set_spawn_impl_for_test()`,
  `projects_locate_set_tool_path_for_test()`,
  `projects_locate_has_pending_for_test()`,
  `projects_locate_reset_for_test()`

## Acceptance Criteria
1. When locate fallback starts and `plocate` exists in `PATH`, it launches that tool; otherwise it falls back to `locate`; if neither exists, the subsystem disables itself and later API calls become no-ops.
2. All locate subprocesses launch asynchronously through `g_subprocess_new()` with a stdout pipe; this subsystem never uses `_sync` subprocess APIs.
3. Starting a locate search converts the user query into a basename glob by interleaving `*` between characters and appending a trailing `*`, so word-initial queries like `bh` match multi-word basenames such as `ba-host` through `plocate -i -b`.
4. When `projects.locate_search_roots` is non-empty, each candidate path must start with one of those configured roots before any exclude or stat work runs; an empty root list leaves the search unbounded.
5. While stdout is being read, each candidate line is processed in this order: trim/skip-empty, search-root prefix check, exclude-glob match, directory existence check, dedupe within the current op, then accept into the oversampled result set.
6. Accepted locate candidates are oversampled up to 200 paths; once that cap is reached, the subprocess is force-exited and the final delivered result set is truncated to 50 rows after ranking.
7. Each locate op has a timeout; when it expires, the op is cancelled and force-exited, and no stale or partial results are delivered back to Projects.
8. Locate callbacks are generation-guarded: if the callback generation no longer matches the current Projects locate generation, the callback frees its resources and makes no model mutation.
9. A locate subprocess exiting with status 1 is treated as an empty result set, not as an error condition.
10. Before results are delivered to Projects, accepted paths are ranked in-process against the original query using cofi's fzf scorer on the basename, with a small shorter-path tiebreak bonus so canonical project roots sort ahead of deep derivative copies when fuzzy scores are otherwise close.

## Notes
- This module is the first nested subsystem under `src/projects/`; parent
  Projects code owns the user-facing trigger policy and consumes this module as
  an async child service.
- The timeout is a deliberate hard bound for UI responsiveness, not a retry
  strategy.
