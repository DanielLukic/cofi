#ifndef PROC_H
#define PROC_H

#include <glib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <X11/Xlib.h>
#include "cofi_tab_provider.h"

#define MAX_PROCS 512
#define MAX_PROC_BASENAME_LEN 128
#define MAX_PROC_CMDLINE_LEN 512

typedef struct AppData AppData;

typedef struct {
    pid_t pid;
    char basename[MAX_PROC_BASENAME_LEN];
    char cmdline[MAX_PROC_CMDLINE_LEN];
    long rss_kb;
    double cpu_pct;
} ProcEntry;

typedef struct {
    pid_t pid;
    unsigned long long jiffies;
} ProcCpuSample;

typedef struct {
    ProcEntry procs[MAX_PROCS];
    int filtered_indices[MAX_PROCS];
    int filtered_scores[MAX_PROCS];
    int proc_count;
    int filtered_count;
    const char *action_candidates[16];
    int action_candidate_count;
    int action_candidate_highlight;
    char snapshot[MAX_PROCS * 24];
    char last_error[256];
    guint refresh_timer_id;
    ProcCpuSample cpu_samples[MAX_PROCS];
    int cpu_sample_count;
    unsigned long long prev_system_jiffies;
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
gboolean proc_execute_action_with_modifiers(AppData *app, const char *entry_text, guint state);
void proc_update_action_candidates(ProcMode *mode, const char *action_spec);
int proc_row_count(AppData *app);
void proc_format_row(AppData *app, int visible_idx, CofiRowCells *out);
const char *proc_match_string(AppData *app, int visible_idx);
const char *proc_row_identity(AppData *app, int visible_idx);
void proc_on_enter(AppData *app);
void proc_on_leave(AppData *app);
void proc_on_query_changed(AppData *app, const char *query);
void proc_on_tick(AppData *app, int generation);
void proc_format_mem_compact(long rss_kb, char *out, size_t out_size);
void proc_format_cpu_pct(double cpu_pct, char *out, size_t out_size);
void proc_fit_name_column(const char *name, char *out, size_t out_size);
void proc_fit_cmd_column(const char *cmdline, int width, char *out, size_t out_size);

#ifdef COFI_TESTING
int proc_signal_from_modifiers_test_hook(guint state);
int proc_parse_pipe_test_hook(const char *input,
                              char *filter_out,
                              size_t filter_out_size,
                              char *action_out,
                              size_t action_out_size);
int proc_resolve_action_test_hook(const char *token, int *signal_out, int *all_out);
int proc_execute_action_test_hook(AppData *app, const char *input, guint state);
void proc_set_kill_impl_test_hook(int (*impl)(pid_t, int));
void proc_snapshot_test_hook(const ProcEntry *procs,
                             int count,
                             char *out,
                             size_t out_size);
int proc_parse_stat_fields_test_hook(const char *stat_line,
                                     unsigned long long *utime,
                                     unsigned long long *stime,
                                     unsigned long long *vsize);
void proc_format_mem_compact_test_hook(long rss_kb, char *out, size_t out_size);
void proc_format_cpu_pct_test_hook(double cpu_pct, char *out, size_t out_size);
void proc_format_name_column_test_hook(const char *name, char *out, size_t out_size);
void proc_format_cmd_column_test_hook(const char *cmdline, int width, char *out, size_t out_size);
void proc_apply_entries_test_hook(AppData *app,
                                  const ProcEntry *entries,
                                  int count);
void proc_set_show_resolvers_test_hook(int (*window_for_pid)(AppData *, pid_t, Window *),
                                       int (*parent_pid)(pid_t),
                                       int max_depth);
#endif

#endif
