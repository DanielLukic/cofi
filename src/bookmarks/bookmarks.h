#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <glib.h>
#include <string.h>

#define BOOKMARKS_MAX_ROWS 20000
#define BOOKMARK_NAME_LEN 256
#define BOOKMARK_FOLDER_LEN 512
#define BOOKMARK_URL_LEN 2048
#define BOOKMARK_PROFILE_LABEL_LEN 128
#define BOOKMARK_PROFILE_DIR_LEN 128
#define BOOKMARKS_ERROR_LEN 256

typedef struct {
    char profile_dir[BOOKMARK_PROFILE_DIR_LEN];
    char profile_label[BOOKMARK_PROFILE_LABEL_LEN];
    char folder_breadcrumb[BOOKMARK_FOLDER_LEN];
    char name[BOOKMARK_NAME_LEN];
    char url[BOOKMARK_URL_LEN];
} BookmarkEntry;

typedef struct {
    GArray *entries;            /* element-type BookmarkEntry */
    GArray *filtered_indices;   /* element-type int, refers into entries */
    char last_error[BOOKMARKS_ERROR_LEN];
} BookmarksMode;

void bookmarks_mode_init(BookmarksMode *mode);
void bookmarks_mode_free(BookmarksMode *mode);
void bookmarks_mode_clear(BookmarksMode *mode);

/* True when scheme is one of http/https/ftp/file (case-insensitive); rejects
 * empty URL and the disallow-list (javascript:, chrome://, chrome-extension://,
 * data:). */
gboolean bookmarks_url_is_allowed(const char *url);

/* Parse Chrome Bookmarks JSON contents for one profile and append url-typed
 * leaves to mode->entries, walking roots.{bookmark_bar, other, synced}.
 * Returns the count appended; 0 on parse failure with error_out populated
 * (caller-visible). Stops appending once mode->entries reaches BOOKMARKS_MAX_ROWS;
 * callers handle the overflow log. */
int bookmarks_parse_chrome(BookmarksMode *mode,
                           const char *contents,
                           const char *profile_dir,
                           const char *profile_label,
                           char *error_out,
                           size_t error_size);

/* Load bookmarks across every Chrome profile (Default first, then by
 * active_time desc, tie-broken case-insensitive name). Skips entries with
 * profile_dir == "Guest Profile". Existing entries are cleared first.
 *
 * Sets mode->last_error to "Failed to load Chrome bookmarks" only when no
 * entries can be loaded at all; per-profile parse failures log a WARN and
 * are skipped.
 */
void bookmarks_load(BookmarksMode *mode);

/* Apply field-weighted filter. Empty query preserves load order. */
void bookmarks_filter(BookmarksMode *mode, const char *query);

/* Render the visible folder breadcrumb: if longer than max_visible characters,
 * left-clip with leading "…" so the deepest folder name stays visible.
 * out_size must be at least 4 bytes. Returns out for convenience. */
const char *bookmarks_truncate_folder_display(const char *breadcrumb,
                                              char *out,
                                              size_t out_size,
                                              int max_visible);

/* Build the match-string corpus for a row: "[bm] <profile> <name> <folder> <url>". */
void bookmarks_format_match_text(const BookmarkEntry *entry,
                                 char *out,
                                 size_t out_size);

#endif /* BOOKMARKS_H */
