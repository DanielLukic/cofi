#ifndef NAMES_STORE_H
#define NAMES_STORE_H

#include <X11/Xlib.h>
#include <stdbool.h>

#include "core/utils/constants.h"
#include "matching/match_entry.h"
#include "x11/window_info.h"

#ifndef APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

typedef struct {
    int match_id;
    char custom_name[MAX_TITLE_LEN];
} NameRecord;

typedef struct {
    NameRecord records[MAX_WINDOWS];
    int count;
    char path[512];
} NamesStore;

void names_store_init(NamesStore *store);
void names_store_init_with_path(NamesStore *store, const char *path);
bool names_store_load(NamesStore *store);
bool names_store_save(const NamesStore *store);
bool names_store_set(NamesStore *store, int match_id, const char *name);
const char *names_store_get_by_match_id(const NamesStore *store, int match_id);
int names_store_find_index_by_match_id(const NamesStore *store, int match_id);
NameRecord *names_store_find_by_custom_name(NamesStore *store, const char *name);
bool names_store_remove_by_match_id(NamesStore *store, int match_id);
int names_store_collect_ids(const NamesStore *store, int *out, int max);

bool names_assign_window(AppData *app, WindowInfo *window, const char *name);
const char *names_get_for_window(const NamesStore *store,
                                 const MatchEntryManager *manager,
                                 const WindowInfo *window);

#endif
