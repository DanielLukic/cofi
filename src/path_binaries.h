#ifndef PATH_BINARIES_H
#define PATH_BINARIES_H

#include <gio/gio.h>

#include "apps.h"

#define MAX_PATH_BINS 4096

typedef struct AppData AppData;

void path_binaries_ensure_loaded(AppData *app);
void path_binaries_invalidate(void);
void path_binaries_filter(const char *query, AppEntry *out, int *out_count);
gboolean path_binaries_is_scanning(void);

void path_binaries_merge_entries_test_hook(AppData *app,
                                           const AppEntry *entries,
                                           int count,
                                           gboolean final_chunk);
void path_binaries_on_monitor_event_test_hook(GFile *file,
                                              GFile *other_file,
                                              GFileMonitorEvent event_type);
void path_binaries_reset_for_tests(void);
gboolean path_binaries_cap_warned_for_tests(void);
int path_binaries_cap_warn_count_for_tests(void);
int path_binaries_count_for_tests(void);

#endif // PATH_BINARIES_H
