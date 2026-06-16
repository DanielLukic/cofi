#include "bookmarks/bookmarks.h"

#include "core/log/log.h"
#include "matching/tier_score.h"
#include "profiles/browser_profiles.h"

#include <json-glib/json-glib.h>
#include <string.h>

void bookmarks_mode_init(BookmarksMode *mode) {
    if (!mode) return;
    memset(mode, 0, sizeof(*mode));
    mode->entries = g_array_new(FALSE, TRUE, sizeof(BookmarkEntry));
    mode->filtered_indices = g_array_new(FALSE, FALSE, sizeof(int));
}

void bookmarks_mode_free(BookmarksMode *mode) {
    if (!mode) return;
    if (mode->entries) {
        g_array_free(mode->entries, TRUE);
        mode->entries = NULL;
    }
    if (mode->filtered_indices) {
        g_array_free(mode->filtered_indices, TRUE);
        mode->filtered_indices = NULL;
    }
}

void bookmarks_mode_clear(BookmarksMode *mode) {
    if (!mode) return;
    if (mode->entries) g_array_set_size(mode->entries, 0);
    if (mode->filtered_indices) g_array_set_size(mode->filtered_indices, 0);
    mode->last_error[0] = '\0';
}

gboolean bookmarks_url_is_allowed(const char *url) {
    if (!url || url[0] == '\0') return FALSE;
    /* Reject the disallow-list first; allow http/https/ftp/file. */
    if (g_ascii_strncasecmp(url, "javascript:", 11) == 0) return FALSE;
    if (g_ascii_strncasecmp(url, "chrome://", 9) == 0) return FALSE;
    if (g_ascii_strncasecmp(url, "chrome-extension://", 19) == 0) return FALSE;
    if (g_ascii_strncasecmp(url, "data:", 5) == 0) return FALSE;
    if (g_ascii_strncasecmp(url, "http://", 7) == 0) return TRUE;
    if (g_ascii_strncasecmp(url, "https://", 8) == 0) return TRUE;
    if (g_ascii_strncasecmp(url, "ftp://", 6) == 0) return TRUE;
    if (g_ascii_strncasecmp(url, "file://", 7) == 0) return TRUE;
    return FALSE;
}

static const char *json_string(JsonObject *obj, const char *key) {
    if (!obj || !json_object_has_member(obj, key)) return "";
    JsonNode *node = json_object_get_member(obj, key);
    return node && JSON_NODE_HOLDS_VALUE(node) ? json_node_get_string(node) : "";
}

static void append_entry(BookmarksMode *mode,
                         const char *profile_dir,
                         const char *profile_label,
                         const char *folder_breadcrumb,
                         const char *name,
                         const char *url) {
    if (!mode || !mode->entries) return;
    if (mode->entries->len >= BOOKMARKS_MAX_ROWS) return;
    BookmarkEntry entry;
    memset(&entry, 0, sizeof(entry));
    g_strlcpy(entry.profile_dir, profile_dir ? profile_dir : "",
              sizeof(entry.profile_dir));
    g_strlcpy(entry.profile_label,
              profile_label && profile_label[0] ? profile_label : (profile_dir ? profile_dir : ""),
              sizeof(entry.profile_label));
    g_strlcpy(entry.folder_breadcrumb, folder_breadcrumb ? folder_breadcrumb : "",
              sizeof(entry.folder_breadcrumb));
    const char *use_name = name && name[0] ? name : (url ? url : "");
    g_strlcpy(entry.name, use_name, sizeof(entry.name));
    g_strlcpy(entry.url, url ? url : "", sizeof(entry.url));
    g_array_append_val(mode->entries, entry);
}

static void walk_node(BookmarksMode *mode,
                      JsonNode *node,
                      const char *profile_dir,
                      const char *profile_label,
                      const char *breadcrumb,
                      int *appended);

static void walk_children(BookmarksMode *mode,
                          JsonArray *children,
                          const char *profile_dir,
                          const char *profile_label,
                          const char *breadcrumb,
                          int *appended) {
    if (!children) return;
    guint len = json_array_get_length(children);
    for (guint i = 0; i < len; i++) {
        if (mode->entries->len >= BOOKMARKS_MAX_ROWS) return;
        JsonNode *child = json_array_get_element(children, i);
        walk_node(mode, child, profile_dir, profile_label, breadcrumb, appended);
    }
}

static void walk_node(BookmarksMode *mode,
                      JsonNode *node,
                      const char *profile_dir,
                      const char *profile_label,
                      const char *breadcrumb,
                      int *appended) {
    if (!node || !JSON_NODE_HOLDS_OBJECT(node)) return;
    JsonObject *obj = json_node_get_object(node);
    const char *type = json_string(obj, "type");
    const char *name = json_string(obj, "name");

    if (strcmp(type, "url") == 0) {
        const char *url = json_string(obj, "url");
        if (bookmarks_url_is_allowed(url)) {
            append_entry(mode, profile_dir, profile_label, breadcrumb, name, url);
            (*appended)++;
        }
        return;
    }

    if (strcmp(type, "folder") == 0) {
        JsonArray *children = json_object_has_member(obj, "children")
            ? json_object_get_array_member(obj, "children")
            : NULL;
        char next[BOOKMARK_FOLDER_LEN];
        if (breadcrumb && breadcrumb[0]) {
            g_snprintf(next, sizeof(next), "%s > %s", breadcrumb, name);
        } else {
            g_strlcpy(next, name, sizeof(next));
        }
        walk_children(mode, children, profile_dir, profile_label, next, appended);
    }
}

static void walk_root_section(BookmarksMode *mode,
                              JsonObject *roots,
                              const char *key,
                              const char *profile_dir,
                              const char *profile_label,
                              int *appended) {
    if (!roots || !json_object_has_member(roots, key)) return;
    JsonNode *node = json_object_get_member(roots, key);
    if (!node || !JSON_NODE_HOLDS_OBJECT(node)) return;
    JsonObject *section = json_node_get_object(node);
    JsonArray *children = json_object_has_member(section, "children")
        ? json_object_get_array_member(section, "children")
        : NULL;
    walk_children(mode, children, profile_dir, profile_label, "", appended);
}

int bookmarks_parse_chrome(BookmarksMode *mode,
                           const char *contents,
                           const char *profile_dir,
                           const char *profile_label,
                           char *error_out,
                           size_t error_size) {
    if (error_out && error_size > 0) error_out[0] = '\0';
    if (!mode || !mode->entries) return 0;
    if (!contents || contents[0] == '\0') {
        if (error_out && error_size > 0)
            g_snprintf(error_out, error_size, "empty bookmarks file");
        return 0;
    }

    GError *error = NULL;
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, contents, -1, &error)) {
        if (error_out && error_size > 0)
            g_snprintf(error_out, error_size, "%s",
                       error ? error->message : "JSON parse failed");
        g_clear_error(&error);
        g_object_unref(parser);
        return 0;
    }

    JsonNode *root = json_parser_get_root(parser);
    JsonObject *root_obj = root && JSON_NODE_HOLDS_OBJECT(root)
        ? json_node_get_object(root) : NULL;
    JsonObject *roots = root_obj && json_object_has_member(root_obj, "roots")
        ? json_object_get_object_member(root_obj, "roots") : NULL;

    int appended = 0;
    walk_root_section(mode, roots, "bookmark_bar", profile_dir, profile_label, &appended);
    walk_root_section(mode, roots, "other", profile_dir, profile_label, &appended);
    walk_root_section(mode, roots, "synced", profile_dir, profile_label, &appended);

    g_object_unref(parser);
    return appended;
}

typedef struct {
    int profile_order; /* 0 for Default, then 1.. by profile sort */
    BrowserProfileEntry entry;
} OrderedProfile;

static int compare_profile_order(gconstpointer a, gconstpointer b) {
    const OrderedProfile *lhs = a;
    const OrderedProfile *rhs = b;
    return lhs->profile_order - rhs->profile_order;
}

void bookmarks_load(BookmarksMode *mode) {
    if (!mode || !mode->entries) return;
    bookmarks_mode_clear(mode);

    BrowserProfileEntry profiles[MAX_BROWSER_PROFILES];
    char enum_err[256] = {0};
    int profile_count = browser_profiles_load_entries(profiles, MAX_BROWSER_PROFILES,
                                                      enum_err, sizeof(enum_err));
    if (profile_count <= 0) {
        g_snprintf(mode->last_error, sizeof(mode->last_error),
                   "Failed to load Chrome bookmarks");
        return;
    }

    /* Build the load order: Default first, then in profile-sort order
     * (already active_time desc / case-insensitive name) from
     * browser_profiles_load_entries, skipping "Guest Profile". */
    GArray *order = g_array_new(FALSE, FALSE, sizeof(OrderedProfile));
    OrderedProfile slot;
    int next_order = 1;
    for (int i = 0; i < profile_count; i++) {
        if (strcmp(profiles[i].profile_dir, "Guest Profile") == 0) continue;
        slot.entry = profiles[i];
        if (strcmp(profiles[i].profile_dir, "Default") == 0) {
            slot.profile_order = 0;
        } else {
            slot.profile_order = next_order++;
        }
        g_array_append_val(order, slot);
    }
    g_array_sort(order, compare_profile_order);

    int total_loaded = 0;
    int overflow_logged = 0;
    for (guint i = 0; i < order->len; i++) {
        const OrderedProfile *op = &g_array_index(order, OrderedProfile, i);
        gchar *path = g_build_filename(g_get_home_dir(), ".config", "google-chrome",
                                       op->entry.profile_dir, "Bookmarks", NULL);
        gchar *contents = NULL;
        GError *gerr = NULL;
        gboolean ok = g_file_get_contents(path, &contents, NULL, &gerr);
        if (!ok) {
            log_warn("bookmarks: failed reading %s: %s",
                     path, gerr ? gerr->message : "unknown error");
            g_clear_error(&gerr);
            g_free(path);
            continue;
        }
        char perr[256] = {0};
        int before = (int)mode->entries->len;
        int added = bookmarks_parse_chrome(mode, contents,
                                           op->entry.profile_dir,
                                           op->entry.name,
                                           perr, sizeof(perr));
        if (added == 0 && perr[0] != '\0') {
            log_warn("bookmarks: failed parsing %s: %s", path, perr);
        }
        total_loaded += (int)mode->entries->len - before;
        g_free(contents);
        g_free(path);

        if (mode->entries->len >= BOOKMARKS_MAX_ROWS && !overflow_logged) {
            log_warn("bookmarks: row cap %d reached; later profiles partially included",
                     BOOKMARKS_MAX_ROWS);
            overflow_logged = 1;
            break;
        }
    }
    g_array_free(order, TRUE);

    if (total_loaded == 0) {
        g_snprintf(mode->last_error, sizeof(mode->last_error),
                   "Failed to load Chrome bookmarks");
    }

    bookmarks_filter(mode, "");
}

void bookmarks_format_match_text(const BookmarkEntry *entry,
                                 char *out,
                                 size_t out_size) {
    if (!out || out_size == 0) return;
    if (!entry) {
        out[0] = '\0';
        return;
    }
    g_snprintf(out, out_size, "[bm] %s %s %s %s",
               entry->profile_label,
               entry->name,
               entry->folder_breadcrumb,
               entry->url);
}

typedef struct {
    int raw_index;
    score_t score;
} BookmarkHit;

static int hit_cmp(const void *lhs, const void *rhs) {
    const BookmarkHit *l = lhs;
    const BookmarkHit *r = rhs;
    if (l->score > r->score) return -1;
    if (l->score < r->score) return 1;
    return l->raw_index - r->raw_index;
}

void bookmarks_filter(BookmarksMode *mode, const char *query) {
    if (!mode || !mode->entries || !mode->filtered_indices) return;
    g_array_set_size(mode->filtered_indices, 0);
    const char *q = query ? query : "";
    int entry_count = (int)mode->entries->len;

    if (q[0] == '\0') {
        for (int i = 0; i < entry_count; i++) {
            g_array_append_val(mode->filtered_indices, i);
        }
        return;
    }

    GArray *hits = g_array_sized_new(FALSE, FALSE, sizeof(BookmarkHit),
                                     (guint)entry_count);
    BookmarkHit hit;
    char match_text[3072];
    for (int i = 0; i < entry_count; i++) {
        const BookmarkEntry *entry = &g_array_index(mode->entries, BookmarkEntry, i);
        bookmarks_format_match_text(entry, match_text, sizeof(match_text));
        score_t score = tier_score_string(q, match_text);
        if (score == SCORE_MIN) continue;
        hit.raw_index = i;
        hit.score = score;
        g_array_append_val(hits, hit);
    }
    qsort(hits->data, hits->len, sizeof(BookmarkHit), hit_cmp);
    for (guint i = 0; i < hits->len; i++) {
        int idx = g_array_index(hits, BookmarkHit, i).raw_index;
        g_array_append_val(mode->filtered_indices, idx);
    }
    g_array_free(hits, TRUE);
}

const char *bookmarks_truncate_folder_display(const char *breadcrumb,
                                              char *out,
                                              size_t out_size,
                                              int max_visible) {
    if (!out || out_size < 4) return "";
    if (!breadcrumb) breadcrumb = "";
    long len = (long)g_utf8_strlen(breadcrumb, -1);
    if (len <= max_visible) {
        g_strlcpy(out, breadcrumb, out_size);
        return out;
    }
    /* Left-clip: keep the last (max_visible - 1) chars, prefix with "…". */
    int keep = max_visible - 1;
    if (keep < 1) keep = 1;
    const char *cut = g_utf8_offset_to_pointer(breadcrumb, len - keep);
    g_snprintf(out, out_size, "…%s", cut);
    return out;
}
