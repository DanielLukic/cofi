#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"

static int pass = 0;
static int fail = 0;
static const char *g_entry_text = "";
static int g_update_display_calls = 0;
static int g_update_scroll_calls = 0;
static int g_hide_calls = 0;
static int g_kill_calls = 0;
static pid_t g_killed_pids[32];
static int g_killed_sigs[32];
static int g_kill_fail_for_pid = -1;
static int g_activate_calls = 0;
static Window g_activated_window = 0;

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
    int score = 1000;
    const char *h = haystack;
    for (const char *n = needle; *n; n++) {
        char nc = (char)g_ascii_tolower((guchar)*n);
        gboolean matched = FALSE;
        while (*h) {
            if (g_ascii_tolower((guchar)*h) == nc) {
                matched = TRUE;
                h++;
                break;
            }
            score--;
            h++;
        }
        if (!matched) {
            return SCORE_MIN;
        }
    }
    return score;
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
    g_hide_calls++;
}

int get_window_pid(Display *display, Window window) {
    (void)display;
    (void)window;
    return 0;
}

void activate_window(Display *display, Window window_id) {
    (void)display;
    g_activate_calls++;
    g_activated_window = window_id;
}

#define COFI_TESTING
#include "../src/process_windows.c"
#include "../src/proc.c"

static int fake_kill(pid_t pid, int sig) {
    if (g_kill_fail_for_pid == (int)pid) {
        errno = EPERM;
        return -1;
    }
    if (g_kill_calls < 32) {
        g_killed_pids[g_kill_calls] = pid;
        g_killed_sigs[g_kill_calls] = sig;
    }
    g_kill_calls++;
    return 0;
}

static ProcEntry make_proc(int pid, const char *name, const char *cmd, long rss_kb) {
    ProcEntry p;
    memset(&p, 0, sizeof(p));
    p.pid = pid;
    g_strlcpy(p.basename, name, sizeof(p.basename));
    g_strlcpy(p.cmdline, cmd, sizeof(p.cmdline));
    p.rss_kb = rss_kb;
    return p;
}

typedef struct {
    pid_t pid;
    Window win;
} ShowMap;

typedef struct {
    pid_t pid;
    pid_t ppid;
} ParentMap;

static ShowMap g_show_map[16];
static int g_show_map_count = 0;
static ParentMap g_parent_map[16];
static int g_parent_map_count = 0;

static int test_find_window_for_pid(AppData *app, pid_t pid, Window *window_out) {
    (void)app;
    for (int i = 0; i < g_show_map_count; i++) {
        if (g_show_map[i].pid == pid) {
            *window_out = g_show_map[i].win;
            return 1;
        }
    }
    return 0;
}

static int test_parent_pid(pid_t pid) {
    for (int i = 0; i < g_parent_map_count; i++) {
        if (g_parent_map[i].pid == pid) {
            return (int)g_parent_map[i].ppid;
        }
    }
    return 1;
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = (TabMode)(TAB_COUNT + 1);
    app->selection.provider_index = 0;
    app->selection.provider_scroll_offset = 0;
    proc_set_kill_impl_test_hook(fake_kill);
    g_hide_calls = 0;
    g_kill_calls = 0;
    g_kill_fail_for_pid = -1;
    g_activate_calls = 0;
    g_activated_window = 0;
    g_show_map_count = 0;
    g_parent_map_count = 0;
    proc_set_show_resolvers_test_hook(test_find_window_for_pid, test_parent_pid, 32);
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
    app.selection.provider_index = 1; /* pid 20 */
    int display_calls_before = g_update_display_calls;
    int scroll_calls_before = g_update_scroll_calls;

    ProcEntry second[3] = {
        make_proc(10, "alpha", "alpha --x changed", 1100),
        make_proc(20, "beta", "beta --new", 1000),
        make_proc(30, "gamma", "gamma --z", 700),
    };
    proc_apply_entries_test_hook(&app, second, 3);

    ASSERT_TRUE("refresh preserves selection by pid", app.selection.provider_index == 1);
    ASSERT_TRUE("unchanged snapshot skips repaint", g_update_display_calls == display_calls_before);
    ASSERT_TRUE("unchanged snapshot skips scroll update", g_update_scroll_calls == scroll_calls_before);
}

static void test_selection_preserve_on_filter_narrowing(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "";

    ProcEntry entries[3] = {
        make_proc(111, "alpha", "alpha keep", 100),
        make_proc(222, "bravo", "bravo keep", 200),
        make_proc(333, "charlie", "charlie drop", 300),
    };
    proc_apply_entries_test_hook(&app, entries, 3);
    app.selection.provider_index = 1; /* pid 222 */

    proc_filter(&app, "br");
    ASSERT_EQ_INT("narrowed filter keeps selected pid", 0, app.selection.provider_index);
    ASSERT_EQ_INT("selected pid still 222", 222,
                  app.proc_mode.procs[app.proc_mode.filtered_indices[app.selection.provider_index]].pid);
}

static void test_selection_resets_when_pid_filtered_out(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "";

    ProcEntry entries[3] = {
        make_proc(111, "alpha", "alpha keep", 100),
        make_proc(222, "bravo", "bravo drop", 200),
        make_proc(333, "charlie", "charlie keep", 300),
    };
    proc_apply_entries_test_hook(&app, entries, 3);
    app.selection.provider_index = 1; /* pid 222 */

    proc_filter(&app, "char");
    ASSERT_EQ_INT("selection resets when previous pid gone", 0, app.selection.provider_index);
    ASSERT_EQ_INT("scroll resets when previous pid gone", 0, app.selection.provider_scroll_offset);
    ASSERT_EQ_INT("new first row is charlie", 333,
                  app.proc_mode.procs[app.proc_mode.filtered_indices[0]].pid);
}

static void test_selection_preserve_while_typing_pipe_action(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "";

    ProcEntry entries[3] = {
        make_proc(5001, "claude", "claude", 100),
        make_proc(5002, "claude", "claude --alt", 90),
        make_proc(5003, "claude-helper", "claude-helper", 80),
    };
    proc_apply_entries_test_hook(&app, entries, 3);
    proc_filter(&app, "claude$");
    app.selection.provider_index = 1; /* second claude pid */

    proc_filter(&app, "claude$ | w");
    ASSERT_EQ_INT("pipe action typing keeps selected pid", 1, app.selection.provider_index);
    ASSERT_EQ_INT("selected pid remains second claude", 5002,
                  app.proc_mode.procs[app.proc_mode.filtered_indices[app.selection.provider_index]].pid);
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

static void test_strict_ranking_basename_then_cmdline(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "qemu!";

    ProcEntry entries[4] = {
        make_proc(200, "qemu", "qemu-system-x86_64", 1024L * 800L),
        make_proc(201, "qemu-helper", "qemu-helper --daemon", 1024L * 1200L),
        make_proc(202, "python", "python /opt/tools/qemu-wrapper.py", 1024L * 5000L),
        make_proc(203, "other", "no-hit", 1024L * 100L),
    };
    proc_apply_entries_test_hook(&app, entries, 4);

    ASSERT_EQ_INT("strict includes 3 matches", 3, app.proc_mode.filtered_count);
    ASSERT_EQ_INT("strict exact basename first",
                  200, app.proc_mode.procs[app.proc_mode.filtered_indices[0]].pid);
    ASSERT_EQ_INT("strict basename substring second",
                  201, app.proc_mode.procs[app.proc_mode.filtered_indices[1]].pid);
    ASSERT_EQ_INT("strict cmdline substring third",
                  202, app.proc_mode.procs[app.proc_mode.filtered_indices[2]].pid);
}

static void test_strict_excludes_fuzzy_only_hits(void) {
    AppData app;
    init_app(&app);

    ProcEntry entries[2] = {
        make_proc(301, "dnsmasq", "dnsmasq resolv dnsmasq.d trust", 1024L * 400L),
        make_proc(302, "qemu", "qemu-system-x86_64", 1024L * 200L),
    };

    g_entry_text = "qemu";
    proc_apply_entries_test_hook(&app, entries, 2);
    ASSERT_EQ_INT("fuzzy mode includes both rows", 2, app.proc_mode.filtered_count);

    g_entry_text = "qemu!";
    proc_filter(&app, g_entry_text);
    ASSERT_EQ_INT("strict mode excludes fuzzy-only row", 1, app.proc_mode.filtered_count);
    ASSERT_EQ_INT("strict keeps true substring row",
                  302, app.proc_mode.procs[app.proc_mode.filtered_indices[0]].pid);
}

static void test_strict_empty_bang_shows_nothing(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "!";

    ProcEntry entries[2] = {
        make_proc(401, "alpha", "alpha", 100),
        make_proc(402, "beta", "beta", 200),
    };
    proc_apply_entries_test_hook(&app, entries, 2);
    ASSERT_EQ_INT("strict empty filter yields no rows", 0, app.proc_mode.filtered_count);
}

static void test_exact_basename_suffix_dollar(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "claude$";

    ProcEntry entries[4] = {
        make_proc(501, "claude", "claude --fast", 1024L * 300L),
        make_proc(502, "claude-helper", "claude-helper --x", 1024L * 999L),
        make_proc(503, "python", "python run claude", 1024L * 5000L),
        make_proc(504, "CLAUDE", "CLAUDE --upper", 1024L * 200L),
    };
    proc_apply_entries_test_hook(&app, entries, 4);

    ASSERT_EQ_INT("exact mode includes only basename equals", 2, app.proc_mode.filtered_count);
    ASSERT_EQ_INT("exact sorted by rss desc first", 501,
                  app.proc_mode.procs[app.proc_mode.filtered_indices[0]].pid);
    ASSERT_EQ_INT("exact case-insensitive includes CLAUDE", 504,
                  app.proc_mode.procs[app.proc_mode.filtered_indices[1]].pid);
}

static void test_exact_empty_dollar_shows_nothing(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "$";

    ProcEntry entries[2] = {
        make_proc(601, "alpha", "alpha", 100),
        make_proc(602, "beta", "beta", 200),
    };
    proc_apply_entries_test_hook(&app, entries, 2);
    ASSERT_EQ_INT("exact empty filter yields no rows", 0, app.proc_mode.filtered_count);
}

static void test_suffix_last_char_wins_between_bang_and_dollar(void) {
    AppData app;
    init_app(&app);

    ProcEntry entries[4] = {
        make_proc(701, "foo", "foo run", 100),
        make_proc(702, "foo-helper", "foo-helper run", 200),
        make_proc(703, "python", "python foo wrapper", 300),
        make_proc(704, "bar", "bar", 400),
    };

    g_entry_text = "foo$!";
    proc_apply_entries_test_hook(&app, entries, 4);
    ASSERT_EQ_INT("foo$! -> strict mode strips only ! and matches foo$ literally",
                  0, app.proc_mode.filtered_count);

    g_entry_text = "foo!$";
    proc_filter(&app, g_entry_text);
    ASSERT_EQ_INT("foo!$ -> exact mode strips only $ and matches foo! literally",
                  0, app.proc_mode.filtered_count);
}

static void test_pipe_parser_extracts_filter_and_action(void) {
    char filter[64];
    char action[64];
    int has_pipe = proc_parse_pipe_test_hook("claude$ | ka", filter, sizeof(filter), action, sizeof(action));
    ASSERT_EQ_INT("pipe parser marks has_pipe", 1, has_pipe);
    ASSERT_TRUE("pipe parser extracts filter", strcmp(filter, "claude$") == 0);
    ASSERT_TRUE("pipe parser extracts action", strcmp(action, "ka") == 0);
}

static void test_action_alias_resolution(void) {
    int sig = 0;
    int all = 0;
    ASSERT_EQ_INT("k -> SIGTERM", 1, proc_resolve_action_test_hook("k", &sig, &all));
    ASSERT_EQ_INT("k signal", SIGTERM, sig);
    ASSERT_EQ_INT("kill all flag false", 0, all);
    ASSERT_EQ_INT("force -> SIGKILL", 1, proc_resolve_action_test_hook("force", &sig, &all));
    ASSERT_EQ_INT("force signal", SIGKILL, sig);
    ASSERT_EQ_INT("hup -> SIGHUP", 1, proc_resolve_action_test_hook("hup", &sig, &all));
    ASSERT_EQ_INT("hup signal", SIGHUP, sig);
    ASSERT_EQ_INT("stop -> SIGSTOP", 1, proc_resolve_action_test_hook("stop", &sig, &all));
    ASSERT_EQ_INT("stop signal", SIGSTOP, sig);
    ASSERT_EQ_INT("cont -> SIGCONT", 1, proc_resolve_action_test_hook("cont", &sig, &all));
    ASSERT_EQ_INT("cont signal", SIGCONT, sig);
}

static void test_compact_all_and_long_all_equivalent(void) {
    int sig = 0;
    int all = 0;
    ASSERT_EQ_INT("ka resolves", 1, proc_resolve_action_test_hook("ka", &sig, &all));
    ASSERT_EQ_INT("ka signal", SIGTERM, sig);
    ASSERT_EQ_INT("ka all", 1, all);
    ASSERT_EQ_INT("kill all resolves", 1, proc_resolve_action_test_hook("kill all", &sig, &all));
    ASSERT_EQ_INT("kill all signal", SIGTERM, sig);
    ASSERT_EQ_INT("kill all all", 1, all);
}

static void test_selected_vs_all_scope(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "alpha";

    ProcEntry entries[3] = {
        make_proc(801, "alpha", "alpha one", 300),
        make_proc(802, "alpha-helper", "alpha helper", 200),
        make_proc(803, "beta", "beta", 100),
    };
    proc_apply_entries_test_hook(&app, entries, 3);
    app.selection.provider_index = 1;

    ASSERT_EQ_INT("selected action success", 1, proc_execute_action_test_hook(&app, "alpha | k", 0));
    ASSERT_EQ_INT("selected action one kill", 1, g_kill_calls);
    ASSERT_EQ_INT("selected action pid", 802, (int)g_killed_pids[0]);
    ASSERT_EQ_INT("selected action signal", SIGTERM, g_killed_sigs[0]);
    ASSERT_EQ_INT("selected action hides window", 1, g_hide_calls);

    init_app(&app);
    g_entry_text = "alpha";
    proc_apply_entries_test_hook(&app, entries, 3);
    ASSERT_EQ_INT("all action success", 1, proc_execute_action_test_hook(&app, "alpha | ka", 0));
    ASSERT_EQ_INT("all action two kills", 2, g_kill_calls);
    ASSERT_EQ_INT("all action hides window", 1, g_hide_calls);
}

static void test_bad_action_token_no_kill(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "alpha";

    ProcEntry entries[1] = {
        make_proc(901, "alpha", "alpha one", 300),
    };
    proc_apply_entries_test_hook(&app, entries, 1);
    ASSERT_EQ_INT("bad action returns false", 0, proc_execute_action_test_hook(&app, "alpha | bogus", 0));
    ASSERT_EQ_INT("bad action no kill", 0, g_kill_calls);
    ASSERT_EQ_INT("bad action no hide", 0, g_hide_calls);
}

static void test_empty_filter_pipe_kill_selected(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "";

    ProcEntry entries[2] = {
        make_proc(910, "alpha", "alpha one", 300),
        make_proc(911, "beta", "beta two", 200),
    };
    proc_apply_entries_test_hook(&app, entries, 2);
    app.selection.provider_index = 0;
    ASSERT_EQ_INT("empty filter pipe selected success", 1, proc_execute_action_test_hook(&app, " | k", 0));
    ASSERT_EQ_INT("empty filter pipe selected pid", 910, (int)g_killed_pids[0]);
}

static void test_pipe_action_candidates_prefix_filtering(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "foo | k";

    ProcEntry entries[1] = {
        make_proc(920, "foo", "foo", 100),
    };
    proc_apply_entries_test_hook(&app, entries, 1);
    ASSERT_TRUE("candidate strip shown for pipe action", app.proc_mode.action_candidate_count > 0);
    ASSERT_TRUE("candidate includes k", strcmp(app.proc_mode.action_candidates[0], "k") == 0);
}

static void test_mem_compact_formatting(void) {
    char out[16];
    proc_format_mem_compact_test_hook(512, out, sizeof(out));
    ASSERT_TRUE("512KiB -> 0M", strcmp(out, "0M") == 0);
    proc_format_mem_compact_test_hook(12 * 1024, out, sizeof(out));
    ASSERT_TRUE("12MiB -> 12M", strcmp(out, "12M") == 0);
    proc_format_mem_compact_test_hook(999 * 1024, out, sizeof(out));
    ASSERT_TRUE("999MiB -> 999M", strcmp(out, "999M") == 0);
    proc_format_mem_compact_test_hook(1024 * 1024, out, sizeof(out));
    ASSERT_TRUE("1024MiB -> 1.0G", strcmp(out, "1.0G") == 0);
    proc_format_mem_compact_test_hook(1700 * 1024, out, sizeof(out));
    ASSERT_TRUE("1700MiB -> 1.7G", strcmp(out, "1.7G") == 0);
    proc_format_mem_compact_test_hook(12L * 1024L * 1024L, out, sizeof(out));
    ASSERT_TRUE("12GiB -> 12G", strcmp(out, "12G") == 0);
}

static void test_cpu_pct_formatting(void) {
    char out[16];
    proc_format_cpu_pct_test_hook(0.0, out, sizeof(out));
    ASSERT_TRUE("cpu 0.0", strcmp(out, "0.0") == 0);
    proc_format_cpu_pct_test_hook(1.5, out, sizeof(out));
    ASSERT_TRUE("cpu 1.5", strcmp(out, "1.5") == 0);
    proc_format_cpu_pct_test_hook(12.3, out, sizeof(out));
    ASSERT_TRUE("cpu 12.3", strcmp(out, "12.3") == 0);
    proc_format_cpu_pct_test_hook(100.0, out, sizeof(out));
    ASSERT_TRUE("cpu 100.0", strcmp(out, "100.0") == 0);
    proc_format_cpu_pct_test_hook(250.0, out, sizeof(out));
    ASSERT_TRUE("cpu 250.0", strcmp(out, "250.0") == 0);
}

static void test_column_truncation_behavior(void) {
    char name_col[32];
    char cmd_col[32];
    proc_format_name_column_test_hook("very-long-process-name", name_col, sizeof(name_col));
    ASSERT_TRUE("name truncated with ellipsis", strstr(name_col, "...") != NULL);
    proc_format_cmd_column_test_hook("this is an extremely long command line", 12, cmd_col, sizeof(cmd_col));
    ASSERT_TRUE("cmd truncated with ellipsis", strcmp(cmd_col + 9, "...") == 0);
}

static void test_show_action_parent_walk_depths(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "alpha";
    ProcEntry entries[1] = { make_proc(1000, "alpha", "alpha", 100) };
    proc_apply_entries_test_hook(&app, entries, 1);

    g_show_map[0] = (ShowMap){ .pid = 1000, .win = 0x1111 };
    g_show_map_count = 1;
    ASSERT_EQ_INT("show depth0 success", 1, proc_execute_action_test_hook(&app, "alpha | show", 0));
    ASSERT_EQ_INT("show depth0 activate called", 1, g_activate_calls);

    init_app(&app);
    g_entry_text = "alpha";
    proc_apply_entries_test_hook(&app, entries, 1);
    g_parent_map[0] = (ParentMap){ .pid = 1000, .ppid = 900 };
    g_show_map[0] = (ShowMap){ .pid = 900, .win = 0x2222 };
    g_parent_map_count = 1;
    g_show_map_count = 1;
    ASSERT_EQ_INT("show depth1 success", 1, proc_execute_action_test_hook(&app, "alpha | show", 0));
    ASSERT_EQ_INT("show depth1 target window", 0x2222, (int)g_activated_window);

    init_app(&app);
    g_entry_text = "alpha";
    proc_apply_entries_test_hook(&app, entries, 1);
    g_parent_map[0] = (ParentMap){ .pid = 1000, .ppid = 900 };
    g_parent_map[1] = (ParentMap){ .pid = 900, .ppid = 800 };
    g_show_map[0] = (ShowMap){ .pid = 800, .win = 0x3333 };
    g_parent_map_count = 2;
    g_show_map_count = 1;
    ASSERT_EQ_INT("show depth2 success", 1, proc_execute_action_test_hook(&app, "alpha | show", 0));
    ASSERT_EQ_INT("show depth2 target window", 0x3333, (int)g_activated_window);
}

static void test_show_action_giveups(void) {
    AppData app;
    init_app(&app);
    g_entry_text = "alpha";
    ProcEntry entries[1] = { make_proc(1000, "alpha", "alpha", 100) };
    proc_apply_entries_test_hook(&app, entries, 1);

    g_parent_map[0] = (ParentMap){ .pid = 1000, .ppid = 1 };
    g_parent_map_count = 1;
    ASSERT_EQ_INT("show gives up at init", 0, proc_execute_action_test_hook(&app, "alpha | show", 0));

    init_app(&app);
    g_entry_text = "alpha";
    proc_apply_entries_test_hook(&app, entries, 1);
    g_parent_map[0] = (ParentMap){ .pid = 1000, .ppid = 999 };
    g_parent_map[1] = (ParentMap){ .pid = 999, .ppid = 998 };
    g_parent_map_count = 2;
    proc_set_show_resolvers_test_hook(test_find_window_for_pid, test_parent_pid, 2);
    ASSERT_EQ_INT("show gives up at depth limit", 0, proc_execute_action_test_hook(&app, "alpha | show", 0));
}

int main(void) {
    test_stat_parsing_fields();
    test_signal_mapping();
    test_selection_preserve_on_refresh();
    test_selection_preserve_on_filter_narrowing();
    test_selection_resets_when_pid_filtered_out();
    test_selection_preserve_while_typing_pipe_action();
    test_snapshot_ignores_cmdline_and_rss();
    test_weighted_basename_priority_sorting();
    test_strict_ranking_basename_then_cmdline();
    test_strict_excludes_fuzzy_only_hits();
    test_strict_empty_bang_shows_nothing();
    test_exact_basename_suffix_dollar();
    test_exact_empty_dollar_shows_nothing();
    test_suffix_last_char_wins_between_bang_and_dollar();
    test_pipe_parser_extracts_filter_and_action();
    test_action_alias_resolution();
    test_compact_all_and_long_all_equivalent();
    test_selected_vs_all_scope();
    test_bad_action_token_no_kill();
    test_empty_filter_pipe_kill_selected();
    test_pipe_action_candidates_prefix_filtering();
    test_mem_compact_formatting();
    test_cpu_pct_formatting();
    test_column_truncation_behavior();
    test_show_action_parent_walk_depths();
    test_show_action_giveups();

    printf("\nProc tests: %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
