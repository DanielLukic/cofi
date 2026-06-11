#ifndef PROJECTS_LOCATE_H
#define PROJECTS_LOCATE_H

#include <gio/gio.h>

typedef struct AppData AppData;
typedef struct CofiConfig CofiConfig;

typedef struct {
    char *path;
} ProjectsLocateResult;

typedef void (*ProjectsLocateResultsCallback)(AppData *app,
                                              const char *query,
                                              guint generation,
                                              const ProjectsLocateResult *results,
                                              int result_count);

void projects_locate_set_results_callback(ProjectsLocateResultsCallback callback);
void projects_locate_search_async(AppData *app, const char *query, guint generation);
void projects_locate_cancel_pending(AppData *app);

#ifdef COFI_TESTING
typedef GSubprocess *(*ProjectsLocateSpawnImpl)(const char *tool_path,
                                                const char *glob_query,
                                                GError **error);
gchar *projects_locate_glob_for_test(const char *query);
gboolean projects_locate_path_excluded_for_test(const CofiConfig *config, const char *path);
void projects_locate_set_spawn_impl_for_test(ProjectsLocateSpawnImpl impl);
void projects_locate_set_tool_path_for_test(const char *tool_path);
gboolean projects_locate_has_pending_for_test(void);
void projects_locate_reset_for_test(void);
#endif

#endif
