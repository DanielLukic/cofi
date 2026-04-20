#include "path_binaries.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "app_data.h"
#include "display.h"
#include "log.h"
#include "match.h"
#include "tab_switching.h"

typedef struct {
    AppData *app;
    AppEntry *entries;
    int count;
    gboolean final_chunk;
    int dir_count;
    gint64 scan_start_us;
} PathMergeChunk;

static AppEntry s_path_entries[MAX_PATH_BINS];
static int s_path_count = 0;
static gboolean s_loaded = FALSE;
static gboolean s_scanning = FALSE;
static gboolean s_rescan_requested = FALSE;
static gboolean s_cap_warned = FALSE;
static int s_cap_warn_count = 0;
static AppData *s_last_app = NULL;

static GHashTable *s_seen_by_name = NULL;  // basename -> full path (first winner)
static GFileMonitor *s_path_monitors[64];
static int s_path_monitor_count = 0;

static int path_entry_cmp(const void *a, const void *b) {
    const AppEntry *left = (const AppEntry *)a;
    const AppEntry *right = (const AppEntry *)b;
    return g_utf8_collate(left->name, right->name);
}

static void sort_path_entries(void) {
    qsort(s_path_entries, (size_t)s_path_count, sizeof(AppEntry), path_entry_cmp);
}

typedef struct {
    AppEntry entry;
    score_t score;
} ScoredPathEntry;

static int scored_path_entry_cmp(const void *a, const void *b) {
    const ScoredPathEntry *left = (const ScoredPathEntry *)a;
    const ScoredPathEntry *right = (const ScoredPathEntry *)b;
    if (left->score > right->score) return -1;
    if (left->score < right->score) return 1;
    return g_utf8_collate(left->entry.name, right->entry.name);
}

static void filter_path_entries(const char *query, AppEntry *out, int *out_count) {
    if (!out || !out_count) return;

    if (!query || query[0] == '\0') {
        int count = s_path_count < MAX_APPS ? s_path_count : MAX_APPS;
        memcpy(out, s_path_entries, (size_t)count * sizeof(AppEntry));
        *out_count = count;
        return;
    }

    /* Score ALL substring-matching entries (up to cache cap MAX_PATH_BINS),
     * then sort, then copy at most MAX_APPS into out[]. Capping during the
     * scoring loop would drop high-score entries that appear late in the
     * alphabetically-sorted cache when >MAX_APPS entries match. */
    ScoredPathEntry scored[MAX_PATH_BINS];
    int scored_count = 0;

    for (int i = 0; i < s_path_count; i++) {
        if (!strcasestr(s_path_entries[i].name, query)) {
            continue;
        }
        score_t score = match(query, s_path_entries[i].name);
        scored[scored_count].entry = s_path_entries[i];
        scored[scored_count].score = score;
        scored_count++;
    }

    qsort(scored, (size_t)scored_count, sizeof(ScoredPathEntry), scored_path_entry_cmp);

    int copy_count = scored_count < MAX_APPS ? scored_count : MAX_APPS;
    for (int i = 0; i < copy_count; i++) {
        out[i] = scored[i].entry;
    }
    *out_count = copy_count;
}

static void warn_path_cache_cap_once(void) {
    if (s_cap_warned) {
        return;
    }

    log_warn("PATH scan: cache cap reached (%d); later PATH entries dropped. Increase MAX_PATH_BINS or reduce PATH.",
             MAX_PATH_BINS);
    s_cap_warned = TRUE;
    s_cap_warn_count++;
}

static void ensure_seen_table(void) {
    if (!s_seen_by_name) {
        s_seen_by_name = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }
}

static void clear_cache(void) {
    s_path_count = 0;
    ensure_seen_table();
    g_hash_table_remove_all(s_seen_by_name);
}

static void maybe_refresh_apps_tab(AppData *app) {
    if (!app || app->current_tab != TAB_APPS || !app->entry) {
        return;
    }

    const char *text = gtk_entry_get_text(GTK_ENTRY(app->entry));
    if (!text || text[0] != '$') {
        return;
    }

    filter_apps(app, text);
    update_display(app);
}

static gboolean path_entry_from_file(const char *full_path, const char *basename, AppEntry *out) {
    if (!full_path || !basename || !out) {
        return FALSE;
    }

    if (!g_file_test(full_path, G_FILE_TEST_EXISTS) ||
        !g_file_test(full_path, G_FILE_TEST_IS_EXECUTABLE) ||
        g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
        return FALSE;
    }

    memset(out, 0, sizeof(*out));
    g_strlcpy(out->name, basename, sizeof(out->name));
    out->source_kind = APP_SOURCE_PATH;
    out->action_id = SYSTEM_ACTION_NONE;
    g_strlcpy(out->exec_path, full_path, sizeof(out->exec_path));
    out->info = NULL;
    return TRUE;
}

static void merge_entries(const AppEntry *entries, int count) {
    if (!entries || count <= 0) {
        return;
    }

    ensure_seen_table();

    for (int i = 0; i < count; i++) {
        const AppEntry *entry = &entries[i];
        if (entry->name[0] == '\0' || entry->exec_path[0] == '\0') {
            continue;
        }

        if (g_hash_table_contains(s_seen_by_name, entry->name)) {
            continue;
        }

        if (s_path_count >= MAX_PATH_BINS) {
            warn_path_cache_cap_once();
            break;
        }

        s_path_entries[s_path_count] = *entry;
        g_hash_table_insert(s_seen_by_name,
                            g_strdup(entry->name),
                            g_strdup(entry->exec_path));
        s_path_count++;
    }
}

static gboolean merge_chunk_cb(gpointer data) {
    PathMergeChunk *chunk = (PathMergeChunk *)data;

    if (chunk->count > 0 && chunk->entries) {
        merge_entries(chunk->entries, chunk->count);
    }

    if (chunk->final_chunk) {
        sort_path_entries();
        s_scanning = FALSE;
        s_loaded = TRUE;

        const double total_ms = (double)(g_get_monotonic_time() - chunk->scan_start_us) / 1000.0;
        log_info("PATH scan: %d entries in %.2fms (%d dirs)",
                 s_path_count,
                 total_ms,
                 chunk->dir_count);
    }

    maybe_refresh_apps_tab(chunk->app ? chunk->app : s_last_app);

    if (chunk->entries) {
        g_free(chunk->entries);
    }
    g_free(chunk);

    if (!s_scanning && s_rescan_requested) {
        s_rescan_requested = FALSE;
        path_binaries_ensure_loaded(s_last_app);
    }

    return G_SOURCE_REMOVE;
}

static void queue_merge_chunk(AppData *app,
                              const AppEntry *entries,
                              int count,
                              gboolean final_chunk,
                              int dir_count,
                              gint64 scan_start_us) {
    PathMergeChunk *chunk = g_new0(PathMergeChunk, 1);
    chunk->app = app;
    chunk->count = count;
    chunk->final_chunk = final_chunk;
    chunk->dir_count = dir_count;
    chunk->scan_start_us = scan_start_us;

    if (count > 0 && entries) {
        chunk->entries = g_new0(AppEntry, count);
        memcpy(chunk->entries, entries, sizeof(AppEntry) * (size_t)count);
    }

    g_idle_add(merge_chunk_cb, chunk);
}

static void clear_monitors(void) {
    for (int i = 0; i < s_path_monitor_count; i++) {
        if (s_path_monitors[i]) {
            g_object_unref(s_path_monitors[i]);
            s_path_monitors[i] = NULL;
        }
    }
    s_path_monitor_count = 0;
}

static void monitor_changed_cb(GFileMonitor *monitor,
                               GFile *file,
                               GFile *other_file,
                               GFileMonitorEvent event_type,
                               gpointer user_data);

static void setup_path_monitors(void) {
    clear_monitors();

    const char *path_env = g_getenv("PATH");
    if (!path_env || path_env[0] == '\0') {
        return;
    }

    gchar **dirs = g_strsplit(path_env, ":", -1);
    int total_dirs = 0;

    for (int i = 0; dirs[i]; i++) {
        if (dirs[i][0] == '\0') {
            continue;
        }

        total_dirs++;
        if (s_path_monitor_count >= 64) {
            continue;
        }

        GFile *dir = g_file_new_for_path(dirs[i]);
        GError *error = NULL;
        GFileMonitor *monitor = g_file_monitor_directory(
            dir,
            G_FILE_MONITOR_NONE,
            NULL,
            &error);
        g_object_unref(dir);

        if (!monitor) {
            log_debug("PATH monitor: skip '%s': %s",
                      dirs[i],
                      error ? error->message : "unknown error");
            g_clear_error(&error);
            continue;
        }

        g_signal_connect(monitor, "changed", G_CALLBACK(monitor_changed_cb), NULL);
        s_path_monitors[s_path_monitor_count++] = monitor;
    }

    if (total_dirs > 64) {
        log_warn("PATH monitor: %d PATH dirs exceed cap 64; monitoring first 64", total_dirs);
    }

    g_strfreev(dirs);
}

static void scan_task_thread(GTask *task,
                             gpointer source_object,
                             gpointer task_data,
                             GCancellable *cancellable) {
    (void)task;
    (void)source_object;
    (void)cancellable;

    AppData *app = (AppData *)task_data;
    const gint64 scan_start_us = g_get_monotonic_time();

    const char *path_env = g_getenv("PATH");
    if (!path_env || path_env[0] == '\0') {
        queue_merge_chunk(app, NULL, 0, TRUE, 0, scan_start_us);
        return;
    }

    gchar **dirs = g_strsplit(path_env, ":", -1);
    int dir_count = 0;

    for (int i = 0; dirs[i]; i++) {
        const char *dir_path = dirs[i];
        if (!dir_path || dir_path[0] == '\0') {
            continue;
        }

        dir_count++;
        const gint64 dir_start_us = g_get_monotonic_time();

        GDir *dir = g_dir_open(dir_path, 0, NULL);
        if (!dir) {
            log_debug("PATH dir '%s': 0 entries in 0.00ms", dir_path);
            continue;
        }

        GArray *chunk = g_array_new(FALSE, FALSE, sizeof(AppEntry));
        const gchar *name = NULL;
        while ((name = g_dir_read_name(dir)) != NULL) {
            if (name[0] == '\0') {
                continue;
            }

            char full_path[1024];
            g_snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);

            AppEntry entry;
            if (path_entry_from_file(full_path, name, &entry)) {
                g_array_append_val(chunk, entry);
                if ((int)chunk->len >= MAX_PATH_BINS) {
                    log_warn("PATH dir '%s' exceeded chunk cap at %d entries; remaining entries in this dir dropped.",
                             dir_path,
                             MAX_PATH_BINS);
                    break;
                }
            }
        }

        g_dir_close(dir);

        const double dir_ms = (double)(g_get_monotonic_time() - dir_start_us) / 1000.0;
        log_debug("PATH dir '%s': %u entries in %.2fms", dir_path, chunk->len, dir_ms);

        if (chunk->len > 0) {
            queue_merge_chunk(app,
                              (const AppEntry *)chunk->data,
                              (int)chunk->len,
                              FALSE,
                              dir_count,
                              scan_start_us);
        }

        g_array_free(chunk, TRUE);
    }

    g_strfreev(dirs);

    queue_merge_chunk(app, NULL, 0, TRUE, dir_count, scan_start_us);
}

static gboolean remove_path_entry_by_name(const char *basename, const char *full_path) {
    if (!basename || basename[0] == '\0') {
        return FALSE;
    }

    char *winner_path = s_seen_by_name ? g_hash_table_lookup(s_seen_by_name, basename) : NULL;
    if (!winner_path) {
        return FALSE;
    }

    if (full_path && full_path[0] != '\0' && g_strcmp0(full_path, winner_path) != 0) {
        return FALSE;
    }

    for (int i = 0; i < s_path_count; i++) {
        if (g_strcmp0(s_path_entries[i].name, basename) != 0) {
            continue;
        }

        for (int j = i; j < s_path_count - 1; j++) {
            s_path_entries[j] = s_path_entries[j + 1];
        }
        s_path_count--;
        g_hash_table_remove(s_seen_by_name, basename);
        return TRUE;
    }

    g_hash_table_remove(s_seen_by_name, basename);
    return FALSE;
}

static gboolean add_path_entry_if_new(const char *full_path) {
    if (!full_path || full_path[0] == '\0') {
        return FALSE;
    }

    if (s_path_count >= MAX_PATH_BINS) {
        warn_path_cache_cap_once();
        return FALSE;
    }

    char *basename = g_path_get_basename(full_path);
    if (!basename || basename[0] == '\0') {
        g_free(basename);
        return FALSE;
    }

    if (g_hash_table_contains(s_seen_by_name, basename)) {
        g_free(basename);
        return FALSE;
    }

    AppEntry entry;
    gboolean ok = path_entry_from_file(full_path, basename, &entry);
    if (!ok) {
        g_free(basename);
        return FALSE;
    }

    s_path_entries[s_path_count++] = entry;
    g_hash_table_insert(s_seen_by_name, basename, g_strdup(full_path));
    sort_path_entries();
    return TRUE;
}

static void path_binaries_on_monitor_event(GFile *file,
                                           GFile *other_file,
                                           GFileMonitorEvent event_type) {
    ensure_seen_table();

    char *path = file ? g_file_get_path(file) : NULL;
    char *other_path = other_file ? g_file_get_path(other_file) : NULL;
    gboolean changed = FALSE;

    switch (event_type) {
        case G_FILE_MONITOR_EVENT_CREATED:
        case G_FILE_MONITOR_EVENT_MOVED_IN:
            changed = add_path_entry_if_new(path);
            break;

        case G_FILE_MONITOR_EVENT_DELETED:
        case G_FILE_MONITOR_EVENT_MOVED_OUT: {
            char *basename = path ? g_path_get_basename(path) : NULL;
            changed = remove_path_entry_by_name(basename, path);
            g_free(basename);
            break;
        }

        case G_FILE_MONITOR_EVENT_RENAMED: {
            char *old_base = path ? g_path_get_basename(path) : NULL;
            if (old_base) {
                changed = remove_path_entry_by_name(old_base, path);
            }
            g_free(old_base);

            if (other_path) {
                changed = add_path_entry_if_new(other_path) || changed;
            }
            break;
        }

        default:
            break;
    }

    if (changed) {
        maybe_refresh_apps_tab(s_last_app);
    }

    g_free(path);
    g_free(other_path);
}

static void monitor_changed_cb(GFileMonitor *monitor,
                               GFile *file,
                               GFile *other_file,
                               GFileMonitorEvent event_type,
                               gpointer user_data) {
    (void)monitor;
    (void)user_data;
    path_binaries_on_monitor_event(file, other_file, event_type);
}

void path_binaries_ensure_loaded(AppData *app) {
    s_last_app = app;

    if (s_loaded || s_scanning) {
        return;
    }

    clear_cache();
    setup_path_monitors();

    s_scanning = TRUE;

    GTask *task = g_task_new(NULL, NULL, NULL, NULL);
    g_task_set_task_data(task, app, NULL);
    g_task_run_in_thread(task, scan_task_thread);
    g_object_unref(task);
}

void path_binaries_invalidate(void) {
    s_loaded = FALSE;
    s_cap_warned = FALSE;
    s_cap_warn_count = 0;

    if (s_scanning) {
        s_rescan_requested = TRUE;
        return;
    }

    path_binaries_ensure_loaded(s_last_app);
}

void path_binaries_filter(const char *query, AppEntry *out, int *out_count) {
    const gint64 start_us = g_get_monotonic_time();
    const char *safe_query = query ? query : "";

    filter_path_entries(safe_query, out, out_count);

    const double elapsed_ms = (double)(g_get_monotonic_time() - start_us) / 1000.0;
    log_debug("path_binaries_filter '%s': %d results in %.2fms",
              safe_query,
              out_count ? *out_count : 0,
              elapsed_ms);
}

gboolean path_binaries_is_scanning(void) {
    return s_scanning;
}

void path_binaries_merge_entries_test_hook(AppData *app,
                                           const AppEntry *entries,
                                           int count,
                                           gboolean final_chunk) {
    ensure_seen_table();

    if (!s_scanning) {
        s_scanning = TRUE;
        s_loaded = FALSE;
    }

    merge_entries(entries, count);

    if (final_chunk) {
        sort_path_entries();
        s_scanning = FALSE;
        s_loaded = TRUE;
    }

    maybe_refresh_apps_tab(app ? app : s_last_app);
}

void path_binaries_on_monitor_event_test_hook(GFile *file,
                                              GFile *other_file,
                                              GFileMonitorEvent event_type) {
    path_binaries_on_monitor_event(file, other_file, event_type);
}

void path_binaries_reset_for_tests(void) {
    clear_cache();
    clear_monitors();
    s_loaded = FALSE;
    s_scanning = FALSE;
    s_rescan_requested = FALSE;
    s_cap_warned = FALSE;
    s_cap_warn_count = 0;
    s_last_app = NULL;
}

gboolean path_binaries_cap_warned_for_tests(void) {
    return s_cap_warned;
}

int path_binaries_cap_warn_count_for_tests(void) {
    return s_cap_warn_count;
}

int path_binaries_count_for_tests(void) {
    return s_path_count;
}
