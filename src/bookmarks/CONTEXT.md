# Bookmarks

## Purpose
Bookmarks lets users browse, fuzzy-filter, open, and slot-assign Chrome
bookmarks read directly from every discovered profile's `Bookmarks` JSON file.

## Boundary

### Owns
- Chrome `Bookmarks` JSON parsing (URL leaves, folder breadcrumbs, root
  sections `bookmark_bar`/`other`/`synced`).
- URL scheme allow-list (http/https/ftp/file) and disallow-list
  (javascript/chrome/chrome-extension/data/empty).
- Bookmark row display, match-string corpus, folder display truncation, and
  tiered ranking over the match-string corpus.
- The optional `BOOKMARKS` tab provider, command surface (`bookmarks`, alias
  `bm`), and bookmark slot payloads.
- Launching the resolved Chrome executable with the owning profile and URL.

### Does Not Own
- Chrome profile discovery — delegates to `profiles/browser_profiles.h`
  (`browser_profiles_load_entries`).
- Chrome argv construction or executable lookup — delegates to
  `profiles/chrome_launch.h` (`chrome_launch_build_argv`,
  `chrome_launch_resolve_executable`).
- Slot storage persistence or global slot-key interpretation.
- Provider registry, command dispatch, modal lifecycle, tab rendering.
- Bookmark add/edit/delete from cofi, non-Chrome bookmark sources, or live
  refresh / caching across tab entries.

## Public Surface
- `bookmarks_provider_register()`
- `bookmarks_mode_init()` / `bookmarks_mode_free()` / `bookmarks_mode_clear()`
- `bookmarks_url_is_allowed()`
- `bookmarks_parse_chrome()`
- `bookmarks_load()`
- `bookmarks_filter()`
- `bookmarks_format_match_text()`
- `bookmarks_truncate_folder_display()`

## Acceptance Criteria
1. Registering the provider creates an optional hidden dynamic tab with id
   `bookmarks`, display label `BOOKMARKS`, hide-on-esc modal behavior,
   initial selection index `0`, and slot storage enabled.
2. The provider shortcut hint is exactly
   `Shortcuts: Enter=Open new window  Shift+Enter=Reuse Chrome  Ctrl+key=Assign slot  Alt+key=Recall slot`.
3. The command surface registers primary command `bookmarks` with alias `bm`,
   description `Switch to bookmarks tab`, help format
   `bookmarks, bm [@SLOT|QUERY]`, and `keeps_open_on_hotkey_auto`.
4. Bare `:bookmarks` exits command mode, records the origin tab for prefix
   return, surfaces the `BOOKMARKS` tab, and leaves cofi open.
5. Tab entry sets the placeholder to `Type to filter bookmarks...` and
   reloads bookmarks from every Chrome profile except `Guest Profile`.
6. Discovery order is Default first, then the order
   `browser_profiles_load_entries` returns (active_time desc, case-insensitive
   name tie-break). Within each profile, rows follow the recursive walk of
   `roots.{bookmark_bar, other, synced}.children`.
7. The parser emits one row per `type == "url"` leaf with
   `{profile_dir, profile_label, folder_breadcrumb, name, url}`. Folder
   breadcrumb joins ancestor folder names with ` > ` (empty for top-level).
   Bookmark name falls back to URL when empty; profile label falls back to
   `profile_dir` when empty. Unknown top-level keys are ignored.
8. The URL scheme allow-list accepts `http`, `https`, `ftp`, `file` and
   rejects `javascript:`, `chrome://`, `chrome-extension://`, `data:`, and
   empty URLs.
9. Total rows are capped at `BOOKMARKS_MAX_ROWS` (20000) across all profiles.
   On overflow a single WARN is logged and remaining profiles are partially
   included.
10. Rows render five cells: `[bm]`, profile label, name, folder breadcrumb
    (display-truncated to 10 chars with leading `…` so the deepest folder
    name stays visible), and URL. Display truncation does not affect the
    match corpus.
11. The match-string corpus is `[bm] <profile_label> <name>
    <folder_breadcrumb> <url>`; the `bm` marker token surfaces every row.
12. Filter ranking uses `tier_score_string()` over the match-string corpus,
    sharing the Windows tab's direct word-boundary tier over indirect fuzzy
    matches; ordering is descending score then original load order.
13. An empty query preserves the load order from criteria 6.
14. With no filtered rows the provider exposes one status row: `last_error`
    rendered as `COFI_ROW_ERROR` when set, otherwise
    `No matching bookmarks found` as non-actionable.
15. Pressing Enter on a row resolves the Chrome executable via
    `chrome_launch_resolve_executable`, builds argv via
    `chrome_launch_build_argv(chrome_path, profile_dir, url, TRUE)`, and
    delegates to `detach_launch_argv_array`, opening the bookmark in a new
    Chrome window. Pressing Shift+Enter builds argv with `new_window == FALSE`
    so Chrome uses its default existing-window/process behavior. The provider
    shortcut hint advertises both actions.
16. Slot payloads use the format `bookmark:chrome:<profile_dir>:<url>`; row
    identity is `bookmark:<profile_dir>:<url>` (no chrome infix); payloads
    that overflow `SLOT_STORE_PAYLOAD_LEN` are rejected with a WARN and not
    assigned. Slot recall parses up to three colon-delimited tokens, taking
    the remainder as the URL so colons inside the URL are preserved.
17. `:bookmarks @SLOT` resolves the payload from the `bookmarks` slot
    namespace and launches it; missing/invalid slots show
    `No matching bookmark.` and keep cofi open.
18. `:bookmarks QUERY` reloads and filters bookmarks, launches the first hit
    on success, and shows `No matching bookmark.` with cofi kept open on no
    hit.
19. Single-profile parse or read failures log
    `bookmarks: failed parsing %s: %s` (or the read equivalent) as WARN, do
    not set `last_error`, and let other profiles still load. A total
    end-to-end failure sets `last_error = "Failed to load Chrome bookmarks"`.

## Notes
- Bookmarks deliberately re-reads every profile's `Bookmarks` file on every
  tab entry. No cache, no `inotify`. The trade-off is documented in
  `docs/gotchas.md`.
- Chrome is the only supported backend. Adding Firefox/etc. would require
  extending the parser, the launch chain, and the slot payload scheme
  together.
- Slot payloads persist only `{profile_dir, url}`; the bookmark display name
  is rediscovered on the next load.
- Bookmarks depends on `src/matching/tier_score.h` for ranking:
  `tier_score_string()` runs over the combined match-string corpus. Changes to
  tier-score semantics change Bookmarks ranking too.
- Bookmarks depends on `src/profiles/chrome_launch.h` for argv assembly and
  executable resolution (`chrome_launch_build_argv`,
  `chrome_launch_resolve_executable`). The `new_window` argv flag at this
  provider's call site distinguishes Enter from Shift+Enter.
- Bookmarks depends on `src/profiles/browser_profiles.h` for profile
  enumeration.
