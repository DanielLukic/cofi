#ifndef FILES_SEARCH_H
#define FILES_SEARCH_H

#include <gio/gio.h>
#include <glib.h>
#include <string.h>

#include "config/config.h"

#ifndef APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#define APPDATA_TYPEDEF_DEFINED
#endif

#define FILES_MAX_VISIBLE 50
#define FILES_MAX_CACHE 50000
#define FILES_QUERY_LEN 256
#define FILES_STATUS_LEN 256

typedef struct {
    GPtrArray *paths;
    int filtered_indices[FILES_MAX_VISIBLE];
    int filtered_count;
    int path_count;
    gboolean cache_valid;
    gboolean loading;
    gboolean dirty;
    gboolean unavailable;
    guint search_generation;
    int tab_mode;
    char query[FILES_QUERY_LEN];
    char status_message[FILES_STATUS_LEN];
} FilesMode;

static inline void init_files_mode(FilesMode *mode) {
    if (!mode) return;
    memset(mode, 0, sizeof(*mode));
    mode->tab_mode = -1;
}

void cleanup_files_mode(AppData *app);

void files_on_enter(AppData *app);
void files_on_leave(AppData *app);
void files_on_query_changed(AppData *app, const char *query);
void files_search_refresh(AppData *app);

int files_row_count(AppData *app);
const char *files_path_at_visible(AppData *app, int visible_idx);
const char *files_match_string(AppData *app, int visible_idx);
const char *files_row_identity(AppData *app, int visible_idx);
const char *files_status_message(AppData *app);
gboolean files_status_is_error(AppData *app);

#ifdef COFI_TESTING
typedef GSubprocess *(*FilesSearchSpawnImpl)(const char *tool_path,
                                             const char *home_path,
                                             gchar **argv,
                                             GError **error);
void files_search_set_spawn_impl_for_test(FilesSearchSpawnImpl impl);
void files_search_set_tool_path_for_test(const char *path);
gboolean files_search_has_pending_for_test(void);
void files_search_reset_for_test(void);
gboolean files_path_excluded_for_test(const CofiConfig *config, const char *path);
#endif

#endif
