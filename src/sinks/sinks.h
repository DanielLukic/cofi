#ifndef SINKS_H
#define SINKS_H

#include <glib.h>
#include <string.h>

#define MAX_SINKS 64
#define MAX_SINK_NAME_LEN 256
#define MAX_SINK_DESC_LEN 256

typedef struct AppData AppData;

typedef struct {
    char name[MAX_SINK_NAME_LEN];
    char description[MAX_SINK_DESC_LEN];
    gboolean is_default;
} SinkEntry;

typedef struct {
    SinkEntry sinks[MAX_SINKS];
    int filtered_indices[MAX_SINKS];
    int sink_count;
    int filtered_count;
    char snapshot[MAX_SINKS * (MAX_SINK_NAME_LEN + 4)];
    char last_error[256];
    gboolean refresh_in_flight;
} SinksMode;

static inline void init_sinks_mode(SinksMode *mode) {
    if (mode) {
        memset(mode, 0, sizeof(*mode));
    }
}
void sinks_refresh_async(AppData *app);
void sinks_filter(AppData *app, const char *filter);
gboolean sinks_switch_name(AppData *app, const char *sink_name);

#ifdef COFI_TESTING
int sinks_parse_inventory_test_hook(const char *inventory,
                                    const char *default_sink,
                                    SinkEntry *out,
                                    int max_out,
                                    char *error_out,
                                    size_t error_size);
void sinks_snapshot_test_hook(const SinkEntry *sinks,
                              int count,
                              char *out,
                              size_t out_size);
#endif

#endif
