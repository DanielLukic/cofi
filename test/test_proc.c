#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"

static int pass = 0;
static int fail = 0;
static const char *g_entry_text = "";
static int g_update_display_calls = 0;
static int g_update_scroll_calls = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

#define ASSERT_EQ_INT(name, a, b) ASSERT_TRUE(name, (a) == (b))

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

const char *gtk_entry_get_text(GtkEntry *entry) {
    (void)entry;
    return g_entry_text;
}

gboolean has_match(const char *query, const char *text) {
    if (!query || query[0] == '\0') return TRUE;
    if (!text) return FALSE;
    return strstr(text, query) != NULL;
}

score_t fzf_fuzzy_match(const char *needle, const char *haystack) {
    if (!needle || needle[0] == '\0') return 0;
    if (!haystack) return SCORE_MIN;
    const char *hit = strstr(haystack, needle);
    if (!hit) return SCORE_MIN;
    return (score_t)(1000 - (int)(hit - haystack));
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

void update_scroll_position(AppData *app) {
    (void)app;
    g_update_scroll_calls++;
}

void hide_window(AppData *app) {
    (void)app;
}

#define COFI_TESTING
#include "../src/proc.c"

static ProcEntry make_proc(int pid, const char *name, const char *cmd, long rss_kb) {
    ProcEntry p;
    memset(&p, 0, sizeof(p));
    p.pid = pid;
    g_strlcpy(p.basename, name, sizeof(p.basename));
    g_strlcpy(p.cmdline, cmd, sizeof(p.cmdline));
    p.rss_kb = rss_kb;
    return p;
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = TAB_PROC;
    app->selection.proc_index = 0;
    app->selection.proc_scroll_offset = 0;
}

static void test_stat_parsing_fields(void) {
    const char *stat_line =
        "1234 (bash) S 1 2 3 4 5 6 7 8 9 10 11 12 777 888 15 16 17 18 19 20 21 999999 23";
    unsigned long long utime = 0, stime = 0, vsize = 0;
    int ok = proc_parse_stat_fields_test_hook(stat_line, &utime, &stime, &vsize);

    ASSERT_EQ_INT("stat parse success", 1, ok);
    ASSERT_TRUE("utime parsed", utime > 0);
    ASSERT_TRUE("stime parsed", stime > 0);
    ASSERT_TRUE("vsize parsed", vsize > 0);
}

static void test_signal_mapping(void) {
    ASSERT_EQ_INT("plain enter -> SIGTERM",
                  SIGTERM, proc_signal_from_modifiers_test_hook(0));
    ASSERT_EQ_INT("shift+enter -> SIGKILL",
                  SIGKILL, proc_signal_from_modifiers_test_hook(GDK_SHIFT_MASK));
    ASSERT_EQ_INT("ctrl+enter -> SIGHUP",
                  SIGHUP, proc_signal_from_modifiers_test_hook(GDK_CONTROL_MASK));
}

static void test_selection_preserve_on_refresh(void) {
    AppData app;
    init_app(&app);
    g_update_display_calls = 0;
    g_update_scroll_calls = 0;
    g_entry_text = "";

    ProcEntry first[3] = {
        make_proc(10, "alpha", "alpha --x", 1000),
        make_proc(20, "beta", "beta --y", 900),
        make_proc(30, "gamma", "gamma --z", 800),
    };
    proc_apply_entries_test_hook(&app, first, 3);
    app.selection.proc_index = 1; /* pid 20 */

    ProcEntry second[3] = {
        make_proc(10, "alpha", "alpha --x changed", 1100),
        make_proc(20, "beta", "beta --new", 1000),
        make_proc(30, "gamma", "gamma --z", 700),
    };
    proc_apply_entries_test_hook(&app, second, 3);

    ASSERT_TRUE("refresh preserves selection by pid", app.selection.proc_index == 1);
    ASSERT_TRUE("unchanged snapshot skips repaint", g_update_display_calls == 1);
    ASSERT_TRUE("unchanged snapshot skips scroll update", g_update_scroll_calls == 1);
}

static void test_snapshot_ignores_cmdline_and_rss(void) {
    ProcEntry procs[2];
    char a[256], b[256];

    procs[0] = make_proc(101, "alpha", "alpha --one", 10);
    procs[1] = make_proc(202, "beta", "beta --two", 20);
    proc_snapshot_test_hook(procs, 2, a, sizeof(a));

    procs[0] = make_proc(101, "alpha", "alpha --changed", 999);
    procs[1] = make_proc(202, "beta", "beta --changed", 999);
    proc_snapshot_test_hook(procs, 2, b, sizeof(b));

    ASSERT_TRUE("snapshot stable for pid+basename only", strcmp(a, b) == 0);
}

static void test_weighted_basename_priority_sorting(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "claude";

    ProcEntry entries[4] = {
        make_proc(101, "python", "python worker claude --session", 1024L * 9000L),
        make_proc(102, "claude", "claude --fast", 1024L * 200L),
        make_proc(103, "java", "java -jar claude-tool.jar", 1024L * 12000L),
        make_proc(104, "claude-helper", "claude-helper --task", 1024L * 300L),
    };
    proc_apply_entries_test_hook(&app, entries, 4);

    ASSERT_EQ_INT("all query matches included", 4, app.proc_mode.filtered_count);
    int first_pid = app.proc_mode.procs[app.proc_mode.filtered_indices[0]].pid;
    int second_pid = app.proc_mode.procs[app.proc_mode.filtered_indices[1]].pid;
    ASSERT_TRUE("top two are basename hits",
                (first_pid == 102 || first_pid == 104) &&
                (second_pid == 102 || second_pid == 104) &&
                first_pid != second_pid);
    ASSERT_EQ_INT("cmdline-only high RSS after basename matches",
                  103, app.proc_mode.procs[app.proc_mode.filtered_indices[2]].pid);
    ASSERT_EQ_INT("cmdline-only lower score/rank last",
                  101, app.proc_mode.procs[app.proc_mode.filtered_indices[3]].pid);
}

int main(void) {
    test_stat_parsing_fields();
    test_signal_mapping();
    test_selection_preserve_on_refresh();
    test_snapshot_ignores_cmdline_and_rss();
    test_weighted_basename_priority_sorting();

    printf("\nProc tests: %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
