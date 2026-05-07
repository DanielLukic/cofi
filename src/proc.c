#include "proc.h"

#include <dirent.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef COFI_PROC_PARSER_TEST
#include "app_data.h"
#include "display.h"
#include "fzf_algo.h"
#include "log.h"
#include "match.h"
#include "selection.h"
#include "window_lifecycle.h"
#endif

static void safe_copy(char *dest, size_t size, const char *src) {
    if (!dest || size == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    g_strlcpy(dest, src, size);
}

static gboolean parse_stat_fields(const char *line,
                                  unsigned long long *utime,
                                  unsigned long long *stime,
                                  unsigned long long *vsize) {
    if (!line || !utime || !stime || !vsize) {
        return FALSE;
    }

    const char *right_paren = strrchr(line, ')');
    if (!right_paren || right_paren[1] == '\0') {
        return FALSE;
    }

    const char *fields = right_paren + 2;
    int field_num = 3;
    char *cursor = (char *)fields;
    char *saveptr = NULL;
    char *token = strtok_r(cursor, " ", &saveptr);
    gboolean got_utime = FALSE, got_stime = FALSE, got_vsize = FALSE;

    while (token) {
        if (field_num == 14) {
            *utime = strtoull(token, NULL, 10);
            got_utime = TRUE;
        } else if (field_num == 15) {
            *stime = strtoull(token, NULL, 10);
            got_stime = TRUE;
        } else if (field_num == 23) {
            *vsize = strtoull(token, NULL, 10);
            got_vsize = TRUE;
            break;
        }
        field_num++;
        token = strtok_r(NULL, " ", &saveptr);
    }

    return got_utime && got_stime && got_vsize;
}

static long parse_rss_kb(FILE *status_file) {
    if (!status_file) {
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), status_file)) {
        if (g_str_has_prefix(line, "VmRSS:")) {
            char *ptr = line + strlen("VmRSS:");
            while (*ptr == ' ' || *ptr == '\t') {
                ptr++;
            }
            return strtol(ptr, NULL, 10);
        }
    }
    return 0;
}

static int compare_rss_desc(const void *a, const void *b) {
    const ProcEntry *pa = a;
    const ProcEntry *pb = b;
    if (pa->rss_kb < pb->rss_kb) return 1;
    if (pa->rss_kb > pb->rss_kb) return -1;
    if (pa->pid < pb->pid) return -1;
    if (pa->pid > pb->pid) return 1;
    return 0;
}

typedef struct {
    int raw_index;
    int final_score;
    long rss_kb;
    pid_t pid;
} ProcFilterHit;

static int compare_filter_hits(const void *a, const void *b) {
    const ProcFilterHit *ha = a;
    const ProcFilterHit *hb = b;

    if (ha->final_score != hb->final_score) {
        return (hb->final_score - ha->final_score);
    }
    if (ha->rss_kb != hb->rss_kb) {
        return (ha->rss_kb < hb->rss_kb) ? 1 : -1;
    }
    if (ha->pid < hb->pid) return -1;
    if (ha->pid > hb->pid) return 1;
    return 0;
}

static const char *find_case_insensitive_substr(const char *haystack, const char *needle) {
    if (!haystack || !needle) {
        return NULL;
    }
    if (needle[0] == '\0') {
        return haystack;
    }

    for (const char *h = haystack; *h; h++) {
        const char *hp = h;
        const char *np = needle;
        while (*hp && *np &&
               g_ascii_tolower((guchar)*hp) == g_ascii_tolower((guchar)*np)) {
            hp++;
            np++;
        }
        if (*np == '\0') {
            return h;
        }
    }
    return NULL;
}

static gboolean equals_case_insensitive(const char *a, const char *b) {
    if (!a || !b) {
        return FALSE;
    }
    while (*a && *b) {
        if (g_ascii_tolower((guchar)*a) != g_ascii_tolower((guchar)*b)) {
            return FALSE;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void build_snapshot(const ProcEntry *procs, int count,
                           char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';
    for (int i = 0; procs && i < count; i++) {
        char line[24];
        g_snprintf(line, sizeof(line), "%d|%s\n", (int)procs[i].pid, procs[i].basename);
        g_strlcat(out, line, out_size);
    }
}

static gboolean read_proc_entry(pid_t pid, ProcEntry *out) {
    if (!out) {
        return FALSE;
    }

    char path[PATH_MAX];
    FILE *f = NULL;
    char comm[MAX_PROC_BASENAME_LEN];
    char cmdraw[MAX_PROC_CMDLINE_LEN];
    size_t cmd_len;
    unsigned long long utime = 0, stime = 0, vsize = 0;
    char stat_line[2048];

    g_snprintf(path, sizeof(path), "/proc/%d/comm", (int)pid);
    f = fopen(path, "r");
    if (!f) {
        return FALSE;
    }
    if (!fgets(comm, sizeof(comm), f)) {
        fclose(f);
        return FALSE;
    }
    fclose(f);
    g_strchomp(comm);

    g_snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    f = fopen(path, "r");
    if (!f) {
        return FALSE;
    }
    cmd_len = fread(cmdraw, 1, sizeof(cmdraw) - 1, f);
    fclose(f);
    cmdraw[cmd_len] = '\0';

    if (cmd_len == 0) {
        return FALSE;
    }

    for (size_t i = 0; i < cmd_len; i++) {
        if (cmdraw[i] == '\0') {
            cmdraw[i] = ' ';
        }
    }
    g_strstrip(cmdraw);
    if (cmdraw[0] == '\0') {
        return FALSE;
    }

    g_snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    f = fopen(path, "r");
    if (f) {
        if (fgets(stat_line, sizeof(stat_line), f)) {
            parse_stat_fields(stat_line, &utime, &stime, &vsize);
        }
        fclose(f);
    }

    g_snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    f = fopen(path, "r");
    long rss_kb = 0;
    if (f) {
        rss_kb = parse_rss_kb(f);
        fclose(f);
    }

    if (rss_kb <= 0 && vsize > 0) {
        rss_kb = (long)(vsize / 1024ULL);
    }
    (void)utime;
    (void)stime;

    out->pid = pid;
    safe_copy(out->basename, sizeof(out->basename), comm);
    safe_copy(out->cmdline, sizeof(out->cmdline), cmdraw);
    out->rss_kb = rss_kb;
    return TRUE;
}

static int load_proc_inventory(ProcEntry *out, int max_out,
                               char *error_out, size_t error_size) {
    if (!out || max_out <= 0) {
        return 0;
    }
    if (error_out && error_size > 0) {
        error_out[0] = '\0';
    }

    DIR *dir = opendir("/proc");
    if (!dir) {
        if (error_out && error_size > 0) {
            g_snprintf(error_out, error_size, "Unable to open /proc");
        }
        return 0;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_out) {
        if (!g_ascii_isdigit((guchar)entry->d_name[0])) {
            continue;
        }
        char *endptr = NULL;
        long pid_long = strtol(entry->d_name, &endptr, 10);
        if (!endptr || *endptr != '\0' || pid_long <= 0) {
            continue;
        }
        ProcEntry proc_entry;
        if (read_proc_entry((pid_t)pid_long, &proc_entry)) {
            out[count++] = proc_entry;
        }
    }
    closedir(dir);

    qsort(out, count, sizeof(ProcEntry), compare_rss_desc);
    if (count == 0 && error_out && error_size > 0) {
        g_snprintf(error_out, error_size, "No userspace processes found");
    }
    return count;
}

static int proc_signal_from_modifiers(guint state) {
    if (state & GDK_SHIFT_MASK) return SIGKILL;
    if (state & GDK_CONTROL_MASK) return SIGHUP;
    return SIGTERM;
}

#ifdef COFI_TESTING
int proc_signal_from_modifiers_test_hook(guint state) {
    return proc_signal_from_modifiers(state);
}

void proc_snapshot_test_hook(const ProcEntry *procs, int count,
                             char *out, size_t out_size) {
    build_snapshot(procs, count, out, out_size);
}

int proc_parse_stat_fields_test_hook(const char *stat_line,
                                     unsigned long long *utime,
                                     unsigned long long *stime,
                                     unsigned long long *vsize) {
    char line_copy[2048];
    safe_copy(line_copy, sizeof(line_copy), stat_line);
    return parse_stat_fields(line_copy, utime, stime, vsize) ? 1 : 0;
}
#endif

#ifndef COFI_PROC_PARSER_TEST
static void set_error(ProcMode *mode, const char *message) {
    if (!mode) return;
    safe_copy(mode->last_error, sizeof(mode->last_error), message);
}

void proc_filter(AppData *app, const char *filter) {
    if (!app) return;
    ProcMode *mode = &app->proc_mode;
    mode->filtered_count = 0;

    ProcFilterHit hits[MAX_PROCS];
    int hit_count = 0;

    typedef enum {
        PROC_MATCH_FUZZY = 0,
        PROC_MATCH_STRICT = 1,
        PROC_MATCH_EXACT = 2,
    } ProcMatchMode;

    ProcMatchMode match_mode = PROC_MATCH_FUZZY;
    char suffix_filter[256];
    suffix_filter[0] = '\0';
    const char *active_filter = filter;

    if (filter) {
        size_t len = strlen(filter);
        if (len > 0 && (filter[len - 1] == '!' || filter[len - 1] == '$')) {
            match_mode = (filter[len - 1] == '$') ? PROC_MATCH_EXACT : PROC_MATCH_STRICT;
            if (len - 1 >= sizeof(suffix_filter)) {
                len = sizeof(suffix_filter);
            }
            memcpy(suffix_filter, filter, len - 1);
            suffix_filter[len - 1] = '\0';
            active_filter = suffix_filter;
        }
    }

    for (int i = 0; i < mode->proc_count; i++) {
        if (hit_count >= MAX_PROCS) break;
        if (match_mode == PROC_MATCH_FUZZY && (!active_filter || active_filter[0] == '\0')) {
            hits[hit_count++] = (ProcFilterHit){
                .raw_index = i,
                .final_score = 0,
                .rss_kb = mode->procs[i].rss_kb,
                .pid = mode->procs[i].pid,
            };
            continue;
        }

        int final_score = 0;

        if (match_mode == PROC_MATCH_EXACT) {
            if (active_filter && active_filter[0] != '\0' &&
                equals_case_insensitive(mode->procs[i].basename, active_filter)) {
                final_score = 1;
            }
        } else if (match_mode == PROC_MATCH_STRICT) {
            if (!active_filter || active_filter[0] == '\0') {
                final_score = 0;
            } else if (equals_case_insensitive(mode->procs[i].basename, active_filter)) {
                final_score = 10000;
            } else if (find_case_insensitive_substr(mode->procs[i].basename, active_filter)) {
                int basename_len = (int)strlen(mode->procs[i].basename);
                int filter_len = (int)strlen(active_filter);
                final_score = 5000 - (basename_len - filter_len);
            } else if (find_case_insensitive_substr(mode->procs[i].cmdline, active_filter)) {
                final_score = 1000;
            }
        } else {
            int basename_score = (int)fzf_fuzzy_match(active_filter, mode->procs[i].basename);
            int cmdline_score = (int)fzf_fuzzy_match(active_filter, mode->procs[i].cmdline);

            if (basename_score > 0) {
                if (cmdline_score < 0) {
                    cmdline_score = 0;
                }
                final_score = basename_score * 10 + cmdline_score;
            } else if (cmdline_score > 0) {
                final_score = cmdline_score;
            }
        }

        if (final_score > 0) {
            hits[hit_count++] = (ProcFilterHit){
                .raw_index = i,
                .final_score = final_score,
                .rss_kb = mode->procs[i].rss_kb,
                .pid = mode->procs[i].pid,
            };
        }
    }

    if ((match_mode != PROC_MATCH_FUZZY || (active_filter && active_filter[0] != '\0')) &&
        hit_count > 1) {
        qsort(hits, (size_t)hit_count, sizeof(hits[0]), compare_filter_hits);
    }

    for (int i = 0; i < hit_count; i++) {
        mode->filtered_indices[i] = hits[i].raw_index;
        mode->filtered_scores[i] = hits[i].final_score;
    }
    mode->filtered_count = hit_count;
}

static void apply_entries(AppData *app, const ProcEntry *entries, int count,
                          const char *error_text) {
    ProcMode *mode = &app->proc_mode;
    char snapshot[sizeof(mode->snapshot)];
    pid_t selected_pid = -1;

    if (app->selection.proc_index >= 0 &&
        app->selection.proc_index < mode->filtered_count) {
        int selected_raw = mode->filtered_indices[app->selection.proc_index];
        if (selected_raw >= 0 && selected_raw < mode->proc_count) {
            selected_pid = mode->procs[selected_raw].pid;
        }
    }

    build_snapshot(entries, count, snapshot, sizeof(snapshot));

    if (count > 0 && strcmp(mode->snapshot, snapshot) == 0 &&
        mode->last_error[0] == '\0') {
        return;
    }

    mode->proc_count = count;
    if (count > 0) {
        memcpy(mode->procs, entries, sizeof(ProcEntry) * count);
        safe_copy(mode->snapshot, sizeof(mode->snapshot), snapshot);
        mode->last_error[0] = '\0';
    } else {
        mode->snapshot[0] = '\0';
        set_error(mode, error_text && error_text[0] ? error_text : "No userspace processes found");
    }

    proc_filter(app, gtk_entry_get_text(GTK_ENTRY(app->entry)));
    app->selection.proc_index = 0;
    app->selection.proc_scroll_offset = 0;
    if (selected_pid > 0) {
        for (int i = 0; i < mode->filtered_count; i++) {
            int raw = mode->filtered_indices[i];
            if (mode->procs[raw].pid == selected_pid) {
                app->selection.proc_index = i;
                break;
            }
        }
    }
    update_scroll_position(app);
    update_display(app);
}

void proc_refresh(AppData *app) {
    if (!app) return;
    ProcEntry parsed[MAX_PROCS];
    char error[256];
    int count = load_proc_inventory(parsed, MAX_PROCS, error, sizeof(error));
    apply_entries(app, parsed, count, error);
}

static gboolean proc_poll_tick(gpointer data) {
    AppData *app = (AppData *)data;
    if (!app || app->current_tab != TAB_PROC) {
        if (app) app->proc_mode.refresh_timer_id = 0;
        return FALSE;
    }
    proc_refresh(app);
    return TRUE;
}

void proc_start_polling(AppData *app) {
    if (!app) return;
    proc_refresh(app);
    if (app->proc_mode.refresh_timer_id == 0) {
        app->proc_mode.refresh_timer_id = g_timeout_add(1500, proc_poll_tick, app);
    }
}

void proc_stop_polling(AppData *app) {
    if (!app || app->proc_mode.refresh_timer_id == 0) return;
    g_source_remove(app->proc_mode.refresh_timer_id);
    app->proc_mode.refresh_timer_id = 0;
}

static int signal_from_modifiers(guint state) {
    return proc_signal_from_modifiers(state);
}

gboolean proc_signal_selected_with_modifiers(AppData *app, guint state) {
    if (!app || app->selection.proc_index < 0 ||
        app->selection.proc_index >= app->proc_mode.filtered_count) {
        return FALSE;
    }

    int raw = app->proc_mode.filtered_indices[app->selection.proc_index];
    if (raw < 0 || raw >= app->proc_mode.proc_count) {
        return FALSE;
    }

    ProcEntry *entry = &app->proc_mode.procs[raw];
    int sig = signal_from_modifiers(state);
    if (kill(entry->pid, sig) != 0) {
        if (errno == EPERM) {
            set_error(&app->proc_mode, "Permission denied sending signal");
        } else if (errno == ESRCH) {
            set_error(&app->proc_mode, "Process no longer exists");
        } else {
            set_error(&app->proc_mode, "Failed to signal process");
        }
        log_warn("proc: kill(%d, %d) failed: %s", (int)entry->pid, sig, strerror(errno));
        update_display(app);
        return FALSE;
    }

    app->proc_mode.last_error[0] = '\0';
    proc_refresh(app);
    hide_window(app);
    return TRUE;
}

#ifdef COFI_TESTING
void proc_apply_entries_test_hook(AppData *app,
                                  const ProcEntry *entries,
                                  int count) {
    apply_entries(app, entries, count, NULL);
}
#endif
#endif
