/*
 * Behavioral ranking tests for the initials-bonus model.
 *
 * Two tests verify the bug fix (FAIL before fix, PASS after):
 *
 *   test_brew_ranking  — "brew" on a Chrome window whose title has
 *                         consecutive B-R-E-W initials must NOT outrank
 *                         the literal "brew | dl" terminal (old max()
 *                         model gave Chrome score 1900 > terminal ~114).
 *
 *   test_chat_ranking  — "chat" on a Chrome window with consecutive
 *                         C-H-A-T initials must NOT outrank the Teams
 *                         window where "Chat" is a literal word.
 *
 * Three regression guards (PASS both before and after fix):
 *
 *   test_composite_acronym_matches   — "gcsnty" spanning class + title
 *                                       still fires initials and appears.
 *
 *   test_scattered_initials_included — scattered (non-consecutive)
 *                                       initials still produce a match
 *                                       and the window appears in results;
 *                                       the fix must not gate on density.
 *
 *   test_short_acronym_ranks_above_scattered — "fd" on "Foo Document"
 *                                       still beats a scattered-fzf-only
 *                                       window.
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

static int mock_desktop = 0;

void update_history(AppData *app)        { (void)app; }
void partition_and_reorder(AppData *app) { (void)app; }
int  get_current_desktop(Display *d)     { (void)d; return mock_desktop; }

const char *get_window_custom_name(const NamedWindowManager *m, Window id) {
    (void)m; (void)id; return NULL;
}

void preserve_selection(AppData *app) { (void)app; }
void restore_selection(AppData *app)  { (void)app; }
void validate_selection(AppData *app) { (void)app; }

/* ---- Module under test ---- */
#include "../src/filter.c"

/* ---- Helpers ---- */

static void reset_app(AppData *app) {
    memset(app, 0, sizeof(*app));
}

static void add_win(AppData *app, Window id, int desktop,
                    const char *instance, const char *title,
                    const char *class_name) {
    int i = app->history_count;
    app->history[i].id      = id;
    app->history[i].desktop = desktop;
    strncpy(app->history[i].instance,   instance,   sizeof(app->history[i].instance)   - 1);
    strncpy(app->history[i].title,      title,       sizeof(app->history[i].title)       - 1);
    strncpy(app->history[i].class_name, class_name,  sizeof(app->history[i].class_name) - 1);
    strncpy(app->history[i].type,       "Normal",    sizeof(app->history[i].type)        - 1);
    app->history_count++;
}

static void print_scores(AppData *app, const char *query) {
    char display[1024];
    printf("\n-- Scores for '%s' --\n", query);
    for (int i = 0; i < app->history_count; i++) {
        compose_display_string(&app->history[i], display, sizeof(display));
        score_t s = match_window(query, &app->history[i]);
        printf("  %.0f  '%s'\n", s, display);
    }
}

/* ------------------------------------------------------------------ */
/* Bug-fix tests: FAIL under the old max() model, PASS after fix.     */
/* ------------------------------------------------------------------ */

/*
 * Chrome title "Big Red Every Window" has consecutive B-R-E-W word-starts —
 * the initials bonus fires.  Short (3-char) words push Chrome's fzf score
 * to ~97 (harder case than longer words ~93).  Under the old max(fzf, 1900)
 * model Chrome scores 1900 and beats the terminal (fzf=114).  Under the
 * additive model: 97+bonus=112 < 114, so terminal ranks first.
 */
static void test_brew_ranking(void) {
    AppData app;
    reset_app(&app);

    /* Chrome: initials B(ig) R(ed) E(very) W(indow) fires for "brew".
     * Shorter words → higher fzf score (~97) — a harder test than
     * longer-word titles (~93).  Still loses to terminal's ~114. */
    add_win(&app, 0x100, 0, "google-chrome",
            "Big Red Every Window - GitHub", "Google-chrome");
    add_win(&app, 0x200, 0, "kitty", "brew | dl", "kitty");

    print_scores(&app, "brew");
    filter_windows(&app, "brew");

    ASSERT_TRUE("brew: terminal ranks #1 (not Chrome)",
                app.filtered_count >= 1 && app.filtered[0].id == 0x200);
    ASSERT_TRUE("brew: Chrome still in results (acronym match preserved)",
                app.filtered_count >= 2 && app.filtered[1].id == 0x100);
}

/*
 * Chrome title "Create Here About Them" has consecutive C-H-A-T
 * word-starts — initials fires for "chat".  Old model scores Chrome 1900
 * above Teams' literal "Chat" fzf score.  New additive model keeps
 * Teams on top.
 */
static void test_chat_ranking(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "msteams",
            "Chat | Claudio | Microsoft Teams", "msteams");
    add_win(&app, 0x200, 0, "google-chrome",
            "Create Here About Them - GitHub", "Google-chrome");

    print_scores(&app, "chat");
    filter_windows(&app, "chat");

    ASSERT_TRUE("chat: Teams ranks #1 (not Chrome)",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

/* ------------------------------------------------------------------ */
/* Regression guards: PASS both before and after fix.                  */
/* ------------------------------------------------------------------ */

/*
 * Full-composite acronym "gcsnty" spans g(oogle-chrome), c(hrome),
 * s(ome), n(ice), t(itle), y(outube) — class + title — and must still
 * match and appear in results.
 */
static void test_composite_acronym_matches(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "google-chrome",
            "Some Nice Title - YouTube", "Google-chrome");

    print_scores(&app, "gcsnty");
    filter_windows(&app, "gcsnty");

    ASSERT_TRUE("gcsnty: full-composite acronym window is in results",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

/*
 * Scattered initials "gyt": g(oogle), y(outube), t(ribute) with several
 * word-starts between consecutive matches.  The fix must not gate on
 * density — this window must still appear in results.
 */
static void test_scattered_initials_included(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "google-chrome",
            "Some YouTube tribute", "Google-chrome");

    print_scores(&app, "gyt");
    filter_windows(&app, "gyt");

    ASSERT_TRUE("gyt: scattered-initials window appears in results (not gated)",
                app.filtered_count >= 1);
}

/*
 * Short genuine acronym "fd" = F(oo) D(ocument) (consecutive word-starts)
 * must rank above a window with only a scattered fzf hit and no initials.
 */
static void test_short_acronym_ranks_above_scattered(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "app", "Foo Document", "app");
    /* 'f' buried in "scaffolding", 'd' in "documentation" — initials fail */
    add_win(&app, 0x200, 0, "code",
            "scaffolding and documentation tools", "Code");

    print_scores(&app, "fd");
    filter_windows(&app, "fd");

    ASSERT_TRUE("fd: Foo Document ranks #1 (acronym over scattered fzf)",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

/* ---- Main ---- */

int main(void) {
    log_set_quiet(true);

    test_brew_ranking();
    test_chat_ranking();
    test_composite_acronym_matches();
    test_scattered_initials_included();
    test_short_acronym_ranks_above_scattered();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return (fail == 0) ? 0 : 1;
}
