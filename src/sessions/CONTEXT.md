# Sessions

## Purpose
Sessions lets users search, resume, rename, and delete Claude and Codex conversation sessions from a hidden provider-backed tab.

## Boundary

### Owns
- Discovery and result aggregation for Claude and Codex `.jsonl` session files.
- Query parsing, asynchronous ripgrep execution, refine filtering, sorting, and display formatting for session rows.
- Resume command construction and terminal launch delegation for supported session sources.
- Guarded session-file delete and append-only rename metadata writes.
- Sessions tab registration, command entrypoint, shortcut handling, and sessions-specific rename/delete overlays.

### Does Not Own
- The provider registry, tab switching framework, command dispatcher, or generic overlay lifecycle.
- Terminal emulator discovery or launch mechanics beyond passing a command to the detach-launch helper.
- Claude or Codex session schema design; this subsystem only reads and appends the records it needs.
- Project/session management outside `.claude/projects` and `.codex/sessions`.

## Public Surface
- `sessions_init()`, `sessions_cancel()`, `sessions_search()`, `sessions_apply_refine()`, `sessions_result_at()`, and `sessions_format_match_text()` for search-mode state and visible results.
- `sessions_build_resume_command()`, `sessions_launch_result()`, `sessions_delete_*()`, `sessions_remove_path()`, `sessions_rename_result()`, and `sessions_rename_path()` for result actions.
- `sessions_parse_query()`, `sessions_extract_json_text()`, and `sessions_extract_name_metadata()` for tested parser behavior.
- `sessions_provider_register()`, `sessions_tab_mode()`, `sessions_provider_remove_path()`, and `sessions_provider_rename_path()`.
- `create_session_rename_overlay_content()`, `handle_session_rename_key_press()`, and `show_session_delete_confirm()`.

## Acceptance Criteria
1. Registering the provider creates a hidden dynamic `SESSIONS` tab with `sessions` as the primary command, `session` as an alias, and the shortcut hint `Shortcuts: Enter=Resume  Ctrl+R=Rename  Ctrl+D/Delete=Delete`.
2. Running the `sessions` command exits command mode, records the origin tab, and surfaces the sessions tab without requiring the tab to be visible by default.
3. Entering the tab sets the search placeholder to `Search sessions: terms | refine...`, cancels any prior search, and starts a new search from the entry text.
4. Leaving the tab cancels any in-flight session search and invalidates pending asynchronous callbacks.
5. A query is split on the first `|`: the left side becomes up to eight whitespace-separated fixed search terms, and the right side becomes the optional refine string.
6. Updating only the refine side reuses existing grouped results; an empty left-side query shows `Type terms to search sessions` and does not spawn rg.
7. Non-empty searches run ripgrep asynchronously over Claude and Codex session roots, restricted to `.jsonl`, case-insensitive fixed-string matching, and excluding file-history and telemetry paths.
8. Vimgrep output is accepted only when the trailing line and column fields are numeric, so paths containing colons remain valid.
9. JSON extraction produces human-readable snippets from Claude message content, Codex payload messages, display/text/message fields, and nested payloads while ignoring raw metadata-only JSON as row snippets.
10. Claude `custom-title`/`agent-name` records and Codex `thread_name_updated` events update the display name; later name metadata wins.
11. Claude project paths are mapped back from slug form to filesystem paths, Codex sessions are labeled as Codex sessions, and history rows remain non-resumable.
12. Results are grouped by source and session id; Claude subagent hits collapse into the parent session row while preserving accumulated hit count.
13. A grouped session becomes visible only after all left-side terms have matched across metadata, extracted text, or fallback metadata.
14. Refine filtering fuzzy-matches the grouped result text after left-side grouping; results sort metadata-full-match rows first, then modified time, capped hit count, and project label.
15. Hit counts are capped at 40 per session, and total visible results are capped at `MAX_SESSION_RESULTS`.
16. Search status reports active searches, no-match completion, match counts, read failures, and line/result limits.
17. Provider rows show six cells: source, hit count, modified time, project label, display name or session id, and snippet; a single non-actionable status row is shown when no results are visible.
18. The provider match string is the formatted grouped search text, and row identity is the backing session path.
19. Pressing Enter on a visible result launches the resume command and hides the modal only when launch succeeds.
20. Claude resume commands use `claude --resume <session_id>`; Codex resume commands use `codex resume <thread_id>`, extracting the UUID suffix from rollout-style Codex ids when present.
21. Resume commands prepend `cd <cwd> &&` only when the result or session file supplies a current working directory that still exists.
22. Delete is available through Delete, keypad Delete, or Ctrl+D only for a selected result row; status rows and inactive tabs do not handle it.
23. Session deletion only removes `.jsonl` files under canonical Claude projects or Codex sessions roots, then removes the row and refreshes selection, scroll, and display.
24. Ctrl+R opens the rename overlay only for Claude or Codex non-history sessions and pre-fills it with the current display name.
25. Claude rename appends both `custom-title` and `agent-name` JSON records; Codex rename appends a `thread_name_updated` event with the extracted thread id.
26. Empty rename submissions close the overlay without changing the session; successful renames update the in-memory result and preserve the renamed session selection even if sorting moves it.

## Notes
- The subsystem deliberately shells out to `rg` for search; cancellation relies on generation checks plus force-killing the subprocess.
- Session deletion is intentionally path-guarded. Do not loosen the canonical root checks without adding equivalent safety coverage.
- Resume launch is delegated to `detach_launch_in_terminal_cmd()` so terminal behavior stays outside this subsystem.
