#ifndef PROC_H
#define PROC_H

#include <glib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>

#define MAX_PROCS 512
#define MAX_PROC_BASENAME_LEN 128
#define MAX_PROC_CMDLINE_LEN 512

typedef struct AppData AppData;

typedef struct {
    pid_t pid;
    char basename[MAX_PROC_BASENAME_LEN];
    char cmdline[MAX_PROC_CMDLINE_LEN];
    long rss_kb;
} ProcEntry;

typedef struct {
    ProcEntry procs[MAX_PROCS];
    int filtered_indices[MAX_PROCS];
    int proc_count;
    int filtered_count;
    char snapshot[MAX_PROCS * 24];
    char last_error[256];
    guint refresh_timer_id;
} ProcMode;

static inline void init_proc_mode(ProcMode *mode) {
    if (mode) {
        memset(mode, 0, sizeof(*mode));
    }
}

void proc_start_polling(AppData *app);
void proc_stop_polling(AppData *app);
void proc_refresh(AppData *app);
void proc_filter(AppData *app, const char *filter);
gboolean proc_signal_selected_with_modifiers(AppData *app, guint state);

#ifdef COFI_TESTING
int proc_signal_from_modifiers_test_hook(guint state);
void proc_snapshot_test_hook(const ProcEntry *procs,
                             int count,
                             char *out,
                             size_t out_size);
int proc_parse_stat_fields_test_hook(const char *stat_line,
                                     unsigned long long *utime,
                                     unsigned long long *stime,
                                     unsigned long long *vsize);
void proc_apply_entries_test_hook(AppData *app,
                                  const ProcEntry *entries,
                                  int count);
#endif

#endif
