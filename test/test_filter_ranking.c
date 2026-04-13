/*
 * Behavioral test: query ranking for 'pgl' in the Windows tab.
 *
 * Reproduces: on desktop [8], querying 'pgl' ranks Kraken Pro (desktop [8])
 * above PGL-Twitch (desktop [1]) due to the +25 workspace bonus overwhelming
 * the tight "PGL" prefix match advantage.
 *
 * Desired behavior: a window whose title literally starts with the exact
 * query abbreviation (consecutive, at a word boundary) should rank first
 * regardless of which workspace it lives on.
 */

#include <stdio.h>
#include <string.h>
#include "../src/app_data.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else       { printf("FAIL: %s\n", name); fail++; } \
} while (0)

/* ---- Stubs ---- */

static int mock_current_desktop = 7;   /* user is on desktop [8] (0-indexed: 7) */

/* history.c */
void update_history(AppData *app)        { (void)app; }
void partition_and_reorder(AppData *app) { (void)app; }

/* x11_utils.c */
int get_current_desktop(Display *d)      { (void)d; return mock_current_desktop; }

/* named_window.c */
const char *get_window_custom_name(const NamedWindowManager *manager, Window id) {
    (void)manager; (void)id; return NULL;
}

/* selection.c */
void preserve_selection(AppData *app)  { (void)app; }
void restore_selection(AppData *app)   { (void)app; }
void validate_selection(AppData *app)  { (void)app; }

/* ---- Module under test ---- */
#include "../src/filter.c"

/* ---- Helpers ---- */

static void reset_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    /* TAB_WINDOWS = 0 already from memset */
    /* WINDOW_ORDER_COFI = 0 already from memset */
}

static void add_win(AppData *app, Window id, int desktop,
                    const char *instance, const char *title,
                    const char *class_name) {
    int i = app->history_count;
    app->history[i].id = id;
    app->history[i].desktop = desktop;
    strncpy(app->history[i].instance, instance,   sizeof(app->history[i].instance)   - 1);
    strncpy(app->history[i].title,    title,       sizeof(app->history[i].title)       - 1);
    strncpy(app->history[i].class_name, class_name, sizeof(app->history[i].class_name) - 1);
    strncpy(app->history[i].type, "Normal", sizeof(app->history[i].type) - 1);
    app->history_count++;
}

/* ---- Score probe: compose display string + run match_window ---- */
/* match_window() is static inside filter.c — accessible because we #include it */

static void print_scores(AppData *app, const char *query) {
    char display[1024];
    printf("\n-- Scores for query '%s' --\n", query);
    for (int i = 0; i < app->history_count; i++) {
        compose_display_string(&app->history[i], display, sizeof(display));
        score_t s = match_window(query, &app->history[i]);
        int bonus = (app->history[i].desktop == mock_current_desktop) ? 5 : 0;
        printf("  raw=%.0f  bonus=%d  final=%.0f  '%s'\n",
               s, bonus, (s > SCORE_MIN ? s + bonus : s), display);
    }
}

/* ---- Tests ---- */

static void test_pgl_twitch_ranks_above_kraken(void) {
    AppData app;
    reset_app(&app);

    /*
     * Windows observed in UI when user typed 'pgl' on desktop [8].
     * Kraken Pro: desktop [8] (0-indexed 7) — same as user → gets +25 bonus
     * PGL-Twitch: desktop [1] (0-indexed 0) — different desktop → no bonus
     */
    add_win(&app, 0x100, 0, "google-chrome",
            "PGL - Twitch - Google Chrome", "Google-chrome");
    add_win(&app, 0x200, 7, "google-chrome",
            "2.17213 | RAVE/USD Spot Trading | Kraken Pro - Google Chrome",
            "Google-chrome");

    print_scores(&app, "pgl");

    filter_windows(&app, "pgl");

    /* PGL-Twitch must be at index 0 */
    ASSERT_TRUE("pgl: PGL-Twitch at index 0",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
    ASSERT_TRUE("pgl: Kraken at index 1",
                app.filtered_count >= 2 && app.filtered[1].id == 0x200);
}

static void test_pgl_twitch_ranks_above_same_desktop_terminal(void) {
    AppData app;
    reset_app(&app);

    /*
     * A terminal on desktop [8] with a path containing p/g/l characters
     * should not outrank a window whose title literally opens with "PGL".
     */
    add_win(&app, 0x100, 0, "google-chrome",
            "PGL - Twitch - Google Chrome", "Google-chrome");
    add_win(&app, 0x300, 7, "kitty",
            "zsh ~/projects/gl-tools", "kitty");

    print_scores(&app, "pgl");

    filter_windows(&app, "pgl");

    ASSERT_TRUE("pgl: PGL-Twitch ranks above same-desktop terminal",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

static void test_pgl_exact_match_beats_workspace_bonus(void) {
    AppData app;
    reset_app(&app);

    /*
     * Full scenario: PGL-Twitch (desktop 1) vs Kraken (desktop 8)
     * vs a terminal (desktop 8).  PGL-Twitch must lead.
     */
    add_win(&app, 0x100, 0, "google-chrome",
            "PGL - Twitch - Google Chrome", "Google-chrome");
    add_win(&app, 0x200, 7, "google-chrome",
            "2.17213 | RAVE/USD Spot Trading | Kraken Pro - Google Chrome",
            "Google-chrome");
    add_win(&app, 0x300, 7, "kitty",
            "zsh ~/projects/gl-tools", "kitty");

    filter_windows(&app, "pgl");

    ASSERT_TRUE("pgl full: PGL-Twitch at index 0",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

/* ---- Main ---- */

int main(void) {
    log_set_quiet(true);   /* suppress log output during tests */

    test_pgl_twitch_ranks_above_kraken();
    test_pgl_twitch_ranks_above_same_desktop_terminal();
    test_pgl_exact_match_beats_workspace_bonus();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return (fail == 0) ? 0 : 1;
}
