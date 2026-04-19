#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/command_api.h"
#include "../src/display_pipeline.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); tests_passed++; } \
    else { printf("FAIL: %s\n", name); tests_failed++; } \
} while (0)

void hide_window(AppData *app) { (void)app; }
void enter_run_mode(AppData *app, const char *cmd) { (void)app; (void)cmd; }
void move_selection_up(AppData *app) { (void)app; }
void move_selection_down(AppData *app) { (void)app; }
char *generate_command_help_text(HelpFormat fmt) { (void)fmt; return NULL; }
gboolean execute_command(const char *cmd, AppData *app) { (void)cmd; (void)app; return TRUE; }
void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

int get_display_columns(AppData *app) { (void)app; return 80; }
int get_dynamic_max_display_lines(AppData *app) { (void)app; return 20; }
int get_scroll_offset(AppData *app) { (void)app; return 0; }
int get_selected_index(AppData *app) { (void)app; return 0; }
gboolean render_display_pipeline(const DisplayPipelineRequest *request, GString *text) {
    (void)request;
    (void)text;
    return TRUE;
}
gint get_window_slot(const HarpoonManager *manager, Window id) {
    (void)manager;
    (void)id;
    return -1;
}
gboolean tab_is_visible(AppData *app, TabMode tab) {
    (void)app;
    (void)tab;
    return TRUE;
}
CofiResult get_x11_property(Display *display, Window window, Atom property, Atom expected_type,
                            unsigned long max_items, Atom *actual_type, int *actual_format,
                            unsigned long *n_items, unsigned char **prop_return) {
    (void)display; (void)window; (void)property; (void)expected_type;
    (void)max_items; (void)actual_type; (void)actual_format; (void)n_items; (void)prop_return;
    return COFI_ERROR;
}

gboolean path_binaries_is_scanning(void) {
    return FALSE;
}

#include "../src/command_parser.c"
#include "../src/command_mode.c"
#include "../src/display.c"

static int lengths_are_sorted(const CommandMode *cmd) {
    for (int i = 1; i < cmd->candidate_count; i++) {
        size_t prev_len = strlen(cmd->candidates[i - 1]);
        size_t cur_len = strlen(cmd->candidates[i]);
        if (prev_len > cur_len) {
            return 0;
        }
        if (prev_len == cur_len && strcmp(cmd->candidates[i - 1], cmd->candidates[i]) > 0) {
            return 0;
        }
    }
    return 1;
}

static int all_candidates_short(const CommandMode *cmd) {
    for (int i = 0; i < cmd->candidate_count; i++) {
        if (strlen(cmd->candidates[i]) > 12) {
            return 0;
        }
    }
    return 1;
}

static int strip_has_candidate(const CommandMode *cmd, const char *name) {
    for (int i = 0; i < cmd->candidate_count; i++) {
        if (strcmp(cmd->candidates[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_is_verb_prefix_accepts_valid(void) {
    ASSERT_TRUE("is_verb_prefix('h')", is_verb_prefix("h") == 1);
    ASSERT_TRUE("is_verb_prefix('ho')", is_verb_prefix("ho") == 1);
    ASSERT_TRUE("is_verb_prefix('h-w')", is_verb_prefix("h-w") == 1);
}

static void test_is_verb_prefix_rejects_invalid(void) {
    ASSERT_TRUE("is_verb_prefix('')", is_verb_prefix("") == 0);
    ASSERT_TRUE("is_verb_prefix('h1')", is_verb_prefix("h1") == 0);
    ASSERT_TRUE("is_verb_prefix(' h')", is_verb_prefix(" h") == 0);
    ASSERT_TRUE("is_verb_prefix('-h')", is_verb_prefix("-h") == 0);
    ASSERT_TRUE("is_verb_prefix('h w')", is_verb_prefix("h w") == 0);
}

static void test_prefix_h_candidates(void) {
    CommandMode cmd = {0};
    command_update_candidates(&cmd, "h");

    ASSERT_TRUE("prefix h has candidates", cmd.candidate_count > 0);
    ASSERT_TRUE("prefix h first candidate is 'h'", cmd.candidate_count > 0 && strcmp(cmd.candidates[0], "h") == 0);
    ASSERT_TRUE("prefix h candidates <=12 chars", all_candidates_short(&cmd));
    ASSERT_TRUE("prefix h sorted by len+alpha", lengths_are_sorted(&cmd));
}

static void test_prefix_hm_matches_only_short_forms(void) {
    CommandMode cmd = {0};
    command_update_candidates(&cmd, "hm");

    ASSERT_TRUE("prefix hm has hm", strip_has_candidate(&cmd, "hm"));
    ASSERT_TRUE("prefix hm has hmw", strip_has_candidate(&cmd, "hmw"));
    ASSERT_TRUE("prefix hm omits horizontal-maximize-window", !strip_has_candidate(&cmd, "horizontal-maximize-window"));
    ASSERT_TRUE("prefix hm candidate count is 2", cmd.candidate_count == 2);
}

static void test_space_exits_prefix_mode(void) {
    CommandMode cmd = {0};
    command_update_candidates(&cmd, "h w");
    ASSERT_TRUE("'h w' hides strip", cmd.candidate_count == 0);
}

static void test_cl_still_shows(void) {
    CommandMode cmd = {0};
    command_update_candidates(&cmd, "cl");
    ASSERT_TRUE("'cl' has candidates", cmd.candidate_count >= 1);
}

static void test_zzzz_hides(void) {
    CommandMode cmd = {0};
    command_update_candidates(&cmd, "zzzz");
    ASSERT_TRUE("'zzzz' hides strip", cmd.candidate_count == 0);
}

static void test_format_candidate_strip_count_zero(void) {
    AppData app = {0};
    GString *out = g_string_new("prefix");

    app.command_mode.candidate_count = 0;
    format_candidate_strip(&app, out);

    ASSERT_TRUE("count=0 appends nothing", strcmp(out->str, "prefix") == 0);
    g_string_free(out, TRUE);
}

static void test_format_candidate_strip_highlight(void) {
    AppData app = {0};
    GString *out = g_string_new("");

    app.command_mode.candidates[0] = "h";
    app.command_mode.candidates[1] = "hm";
    app.command_mode.candidates[2] = "help";
    app.command_mode.candidate_count = 3;
    app.command_mode.candidate_highlight = 1;

    format_candidate_strip(&app, out);

    ASSERT_TRUE("highlighted candidate wrapped in brackets", strstr(out->str, "[ hm ]") != NULL);
    g_string_free(out, TRUE);
}

int main(void) {
    printf("Command candidate tests\n");
    printf("=======================\n\n");

    test_is_verb_prefix_accepts_valid();
    test_is_verb_prefix_rejects_invalid();
    test_prefix_h_candidates();
    test_prefix_hm_matches_only_short_forms();
    test_space_exits_prefix_mode();
    test_cl_still_shows();
    test_zzzz_hides();
    test_format_candidate_strip_count_zero();
    test_format_candidate_strip_highlight();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
