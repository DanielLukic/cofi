#ifndef APPS_H
#define APPS_H

#include <gio/gdesktopappinfo.h>
#include "match.h"

#define MAX_APPS 512

typedef enum {
    APP_SOURCE_DESKTOP = 0,
    APP_SOURCE_SYSTEM = 1,
    APP_SOURCE_PATH = 2,
} AppSourceKind;

typedef enum {
    SYSTEM_ACTION_NONE = 0,
    SYSTEM_ACTION_LOCK,
    SYSTEM_ACTION_SUSPEND,
    SYSTEM_ACTION_HIBERNATE,
    SYSTEM_ACTION_LOGOUT,
    SYSTEM_ACTION_REBOOT,
    SYSTEM_ACTION_SHUTDOWN,
} SystemActionId;

typedef struct {
    char name[256];
    char generic_name[256];
    char keywords[512];
    AppSourceKind source_kind;
    SystemActionId action_id;
    char exec_path[512];
    GAppInfo *info;  /* owned by GIO list; valid until apps_unload() */
} AppEntry;

/* Load all launchable desktop apps, sorted alphabetically. */
void apps_load(void);

/* Free GIO resources (called at reload or shutdown). */
void apps_unload(void);

/* Filter src entries by query into out. Pure: no GIO calls. */
void apps_filter_entries(const char *query,
                         AppEntry *src, int src_count,
                         AppEntry *out, int *out_count);

/* Sort entries alphabetically by name (in-place). Pure. */
void apps_sort_entries(AppEntry *entries, int count);

/* Fill out/out_count from the loaded list filtered by query. */
void apps_filter(const char *query, AppEntry *out, int *out_count);

/* Launch app detached. Logs on failure. */
void apps_launch(const AppEntry *entry);

#endif /* APPS_H */
