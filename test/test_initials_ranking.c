/*
 * Behavioral ranking tests for the tiered scoring model.
 *
 * The model has two tiers:
 *   TIER_DIRECT   — query appears as a contiguous run at a word boundary
 *                   in the composite display string.  Score += TIER_DIRECT_BASE.
 *   TIER_INDIRECT — everything else that fzf_has_match passes.
 *
 * workspace_bonus is 1 (pure tiebreaker, never overrides a real score gap).
 *
 * Must-pass ranking invariants (real usage bar):
 *   test_brew_ranking        — "brew" → "brew | dl" #1 over Chrome word-starts
 *   test_chat_ranking        — "chat" → "Chat | ... Teams" #1 over Chrome word-starts
 *   test_ch_ranking          — "ch" → "Chat | ... Teams" #1 even when it is on a
 *                               different desktop than the google-chrome windows
 *
 * Adversarial cases (sam's single-char-word titles):
 *   test_chat_adversarial    — "chat" → "Chat | ... Teams" beats "C H A T" Chrome
 *   test_brew_adversarial    — "brew" → "brew | dl" beats "B R E W" Chrome
 *
 * Inclusion guards (scattered initials must still appear):
 *   test_gyt_included        — "gyt" matches a YouTube window (TIER_INDIRECT)
 *
 * Regression:
 *   test_fd_ranking          — "fd" on "Foo Document" beats mid-word scatter
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

const char *match_entry_get_custom_name(const MatchEntryManager *m, Window id) {
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
/* Must-pass ranking invariants                                         */
/* ------------------------------------------------------------------ */

/*
 * "brew" on a Chrome window with B-R-E-W word-starts must NOT outrank
 * the literal "brew | dl" terminal.  The terminal has a direct
 * word-boundary contiguous match → TIER_DIRECT; Chrome only has
 * scattered initials → TIER_INDIRECT.
 */
static void test_brew_ranking(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "google-chrome",
            "Big Red Every Window - GitHub", "Google-chrome");
    add_win(&app, 0x200, 0, "kitty", "brew | dl", "kitty");

    print_scores(&app, "brew");
    filter_windows(&app, "brew");

    ASSERT_TRUE("brew: terminal ranks #1 (not Chrome)",
                app.filtered_count >= 1 && app.filtered[0].id == 0x200);
    ASSERT_TRUE("brew: Chrome still in results (TIER_INDIRECT match preserved)",
                app.filtered_count >= 2 && app.filtered[1].id == 0x100);
}

/*
 * "chat" on a Chrome window with C-H-A-T word-starts must NOT outrank
 * the Teams window where "Chat" is a literal word at a word boundary.
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

/*
 * "ch" on a list where every window is google-chrome EXCEPT one Teams
 * window whose title starts "Chat".  The Teams window is on desktop 1
 * (non-current); Chrome windows are on desktop 0 (current).
 *
 * Both match "ch" as a contiguous word-boundary run (TIER_DIRECT):
 *   Teams:  'C' in "Chat" after space  → fzf uses BONUS_BOUNDARY_WHITE
 *   Chrome: 'c' in "chrome" after '-' → fzf uses BONUS_BOUNDARY_DELIMITER
 *
 * BONUS_BOUNDARY_WHITE(10) > BONUS_BOUNDARY_DELIMITER(9), so Teams has
 * ~2pt higher fzf.  workspace_bonus must be ≤ 1 so it cannot override
 * that gap.  FAIL if workspace_bonus = 5 (old value).
 */
static void test_ch_ranking(void) {
    AppData app;
    reset_app(&app);
    mock_desktop = 0;

    /* Teams on desktop 1 (not current) */
    add_win(&app, 0x100, 1, "msteams",
            "Chat | Claudio | Microsoft Teams", "msteams");
    /* Chrome windows on desktop 0 (current) */
    add_win(&app, 0x200, 0, "google-chrome",
            "specdd/cofi - GitHub - Google Chrome", "Google-chrome");
    add_win(&app, 0x300, 0, "google-chrome",
            "some other page - Google Chrome", "Google-chrome");

    print_scores(&app, "ch");
    filter_windows(&app, "ch");

    ASSERT_TRUE("ch: Teams Chat ranks #1 even though on different desktop",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

/* ------------------------------------------------------------------ */
/* Adversarial cases: sam's single-char-word titles                    */
/* ------------------------------------------------------------------ */

/*
 * "C H A T" (each char a separate word) gives fzf ~105 because each
 * char hits a word-start with BONUS_BOUNDARY_WHITE.  It also fires the
 * old initials bonus.  Under the old model (fzf+15 = 120 > 114) Teams
 * lost.  Under the tier model "C H A T" is TIER_INDIRECT (no contiguous
 * run) and Teams is TIER_DIRECT → Teams wins by construction.
 */
static void test_chat_adversarial(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "msteams",
            "Chat | Claudio | Microsoft Teams", "msteams");
    add_win(&app, 0x200, 0, "google-chrome",
            "C H A T - GitHub", "Google-chrome");

    print_scores(&app, "chat");
    filter_windows(&app, "chat");

    ASSERT_TRUE("chat adversarial: Teams ranks #1 over 'C H A T' Chrome",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

/*
 * "B R E W" single-char-word title — same adversarial structure as
 * "C H A T".  fzf(brew, "B R E W ...") ~105 + old initials bonus = 120
 * beats terminal 114.  Tier model: terminal is TIER_DIRECT, Chrome
 * TIER_INDIRECT → terminal wins by construction.
 */
static void test_brew_adversarial(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "kitty", "brew | dl", "kitty");
    add_win(&app, 0x200, 0, "google-chrome",
            "B R E W - GitHub", "Google-chrome");

    print_scores(&app, "brew");
    filter_windows(&app, "brew");

    ASSERT_TRUE("brew adversarial: terminal ranks #1 over 'B R E W' Chrome",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

/* ------------------------------------------------------------------ */
/* Inclusion guard: scattered initials must still appear               */
/* ------------------------------------------------------------------ */

/*
 * "gyt" — g(oogle), y(ouTube), t(ribute) — scattered word-starts with
 * gaps.  The tier model must not gate on density: this window must
 * appear in results as TIER_INDIRECT via fzf_has_match.
 */
static void test_gyt_included(void) {
    AppData app;
    reset_app(&app);

    /* Realistic YouTube window title */
    add_win(&app, 0x100, 0, "google-chrome",
            "Synthwave Mix - YouTube - Google Chrome", "Google-chrome");

    print_scores(&app, "gyt");
    filter_windows(&app, "gyt");

    ASSERT_TRUE("gyt: YouTube window appears in results (TIER_INDIRECT, not gated)",
                app.filtered_count >= 1);
}

/* ------------------------------------------------------------------ */
/* Regression: "fd" acronym over mid-word scatter                      */
/* ------------------------------------------------------------------ */

/*
 * "fd" = F(oo) D(ocument) — both chars at word-starts (TIER_INDIRECT
 * but high fzf).  Must rank above a window where 'f' and 'd' only
 * appear mid-word (lower fzf, TIER_INDIRECT).
 */
static void test_fd_ranking(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "app", "Foo Document", "app");
    add_win(&app, 0x200, 0, "code",
            "scaffolding and documentation tools", "Code");

    print_scores(&app, "fd");
    filter_windows(&app, "fd");

    ASSERT_TRUE("fd: Foo Document ranks #1 (word-starts over mid-word scatter)",
                app.filtered_count >= 1 && app.filtered[0].id == 0x100);
}

/* ---- Main ---- */

int main(void) {
    log_set_quiet(true);

    test_brew_ranking();
    test_chat_ranking();
    test_ch_ranking();
    test_chat_adversarial();
    test_brew_adversarial();
    test_gyt_included();
    test_fd_ranking();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return (fail == 0) ? 0 : 1;
}
