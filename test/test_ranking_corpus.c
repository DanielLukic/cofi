/*
 * Ranking eval corpus — 16-window fixture from the user's real window list.
 *
 * Covers all 5 match rules:
 *   RULE 1  AT ALL          — fzf_has_match subsequence (inclusion only)
 *   RULE 2  RIGHT ORDER     — fzf handles in-order subsequence
 *   RULE 3  WORD SNIPPET    — contiguous substring anywhere (TIER_DIRECT if at word boundary)
 *   RULE 4  WORD START      — contiguous run at word boundary (TIER_DIRECT)
 *   RULE 5  WORD-STARTS ACROSS — initials at word-starts with consecutive density
 *
 * Run baseline with: mise run test-target test_ranking_corpus
 * Expected: some tests FAIL before signal fixes (ch, gcse).
 */

#include <stdio.h>
#include <string.h>
#include "core/app/app_data.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_RANK1(name, app_ptr, expected_id) do { \
    if ((app_ptr)->filtered_count >= 1 && (app_ptr)->filtered[0].id == (expected_id)) { \
        printf("PASS: %s\n", name); pass++; \
    } else { \
        printf("FAIL: %s  (got id=0x%lx count=%d)\n", \
               name, \
               (app_ptr)->filtered_count >= 1 ? (unsigned long)(app_ptr)->filtered[0].id : 0, \
               (app_ptr)->filtered_count); \
        fail++; \
    } \
} while (0)

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else       { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#define ASSERT_IN_TOP2(name, app_ptr, id_a, id_b) do { \
    int found_a = 0, found_b = 0; \
    for (int _i = 0; _i < 2 && _i < (app_ptr)->filtered_count; _i++) { \
        if ((app_ptr)->filtered[_i].id == (id_a)) found_a = 1; \
        if ((app_ptr)->filtered[_i].id == (id_b)) found_b = 1; \
    } \
    if (found_a && found_b) { printf("PASS: %s\n", name); pass++; } \
    else { \
        printf("FAIL: %s  (top2: 0x%lx 0x%lx)\n", name, \
               (app_ptr)->filtered_count >= 1 ? (unsigned long)(app_ptr)->filtered[0].id : 0, \
               (app_ptr)->filtered_count >= 2 ? (unsigned long)(app_ptr)->filtered[1].id : 0); \
        fail++; \
    } \
} while (0)

#define ASSERT_NOT_IN_TOP2(name, app_ptr, excluded_id) do { \
    int found = 0; \
    for (int _i = 0; _i < 2 && _i < (app_ptr)->filtered_count; _i++) { \
        if ((app_ptr)->filtered[_i].id == (excluded_id)) found = 1; \
    } \
    if (!found) { printf("PASS: %s\n", name); pass++; } \
    else { \
        printf("FAIL: %s  (0x%lx unexpectedly in top2: 0x%lx 0x%lx)\n", name, \
               (unsigned long)(excluded_id), \
               (app_ptr)->filtered_count >= 1 ? (unsigned long)(app_ptr)->filtered[0].id : 0, \
               (app_ptr)->filtered_count >= 2 ? (unsigned long)(app_ptr)->filtered[1].id : 0); \
        fail++; \
    } \
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
#include "matching/filter.c"

/* ---- Fixture ---- */

/* Window IDs */
#define WIN_COINER_TERM1  0x01  /* d1 mate-terminal "coiner-dev | ✳ coiner master agent" */
#define WIN_COINER_TERM2  0x02  /* d1 mate-terminal "claude-coiner:develop* signals-view — Terminal" */
#define WIN_KUPPEL        0x03  /* d0 google-chrome "kuppelfenster für flachdach..." */
#define WIN_BIOKAT        0x04  /* d0 google-chrome "biokat - Google Search - Google Chrome" */
#define WIN_SPECDD        0x05  /* d0 google-chrome "specdd/specdd: Specification-Driven..." */
#define WIN_COFI_ISSUES   0x06  /* d4 google-chrome "Cofi - The Comfortable Window Switcher › Issues" */
#define WIN_SIKA_TEICH    0x07  /* d0 google-chrome "SIKA PVC Teichfolie..." */
#define WIN_TSUNAMI       0x08  /* d0 thunderbird-esr "Tsunami IMAP - Mozilla Thunderbird" class=Mail */
#define WIN_COFI_TERM     0x09  /* d4 mate-terminal "cofi | main" */
#define WIN_ZCREW         0x0A  /* d5 mate-terminal "zcrew | main" */
#define WIN_YT_ANTHROPIC  0x0B  /* d0 google-chrome "Anthropic Just Dropped... YouTube" */
#define WIN_KRAKEN        0x0C  /* d1 google-chrome "Hyperliquid... Kraken Pro" */
#define WIN_SLACK         0x0D  /* d3 google-chrome "Daniel Dario Lukic (DM) - GLSCE - Slack" */
#define WIN_TEAMS         0x0E  /* d6 google-chrome "Chat | Lukic, Daniel (You) | Microsoft Teams" */
#define WIN_YT_SOFTENG    0x0F  /* d4 google-chrome "Software engineering at the tipping point - YouTube" */
#define WIN_SCREENSHOT    0x10  /* d4 mate-screenshot "Save Screenshot" */

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

/* Load all 16 fixture windows into app->history */
static void load_fixture(AppData *app) {
    reset_app(app);
    /* Ordering: MRU-first (most recently used first) — arbitrary for corpus tests */
    add_win(app, WIN_COINER_TERM1, 1,
            "mate-terminal",
            "coiner-dev | \xe2\x9c\xb3 coiner master agent",
            "mate-terminal");
    add_win(app, WIN_COINER_TERM2, 1,
            "mate-terminal",
            "claude-coiner:develop* signals-view \xe2\x80\x94 Terminal",
            "mate-terminal");
    add_win(app, WIN_KUPPEL, 0,
            "google-chrome",
            "kuppelfenster f\xc3\xbcr flachdach 50x50 - Google Search - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_BIOKAT, 0,
            "google-chrome",
            "biokat - Google Search - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_SPECDD, 0,
            "google-chrome",
            "specdd/specdd: Specification-Driven Development framework that enables humans and AI agents to build better software - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_COFI_ISSUES, 4,
            "google-chrome",
            "Cofi - The Comfortable Window Switcher Issues - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_SIKA_TEICH, 0,
            "google-chrome",
            "SIKA PVC Teichfolie 1,00 mm steingrau mit Vlies 500g/m2 im Set | TEICHFOLIE.de - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_TSUNAMI, 0,
            "thunderbird-esr",
            "Tsunami IMAP - Mozilla Thunderbird",
            "Mail");
    add_win(app, WIN_COFI_TERM, 4,
            "mate-terminal",
            "cofi | main",
            "mate-terminal");
    add_win(app, WIN_ZCREW, 5,
            "mate-terminal",
            "zcrew | main",
            "mate-terminal");
    add_win(app, WIN_YT_ANTHROPIC, 0,
            "google-chrome",
            "Anthropic Just Dropped the Biggest Subagent Upgrade Yet - YouTube - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_KRAKEN, 1,
            "google-chrome",
            "Hyperliquid (HYPE) to USD Spot Trading - Price & Chart | Kraken Pro - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_SLACK, 3,
            "google-chrome",
            "Daniel Dario Lukic (DM) - GLSCE - Slack - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_TEAMS, 6,
            "google-chrome",
            "Chat | Lukic, Daniel (You) | Microsoft Teams - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_YT_SOFTENG, 4,
            "google-chrome",
            "Software engineering at the tipping point - YouTube - Google Chrome",
            "Google-chrome");
    add_win(app, WIN_SCREENSHOT, 4,
            "mate-screenshot",
            "Save Screenshot",
            "mate-screenshot");
}

/* Print scores for a query against the fixture (diagnostic only) */
static void print_scores(AppData *app, const char *query) {
    char display[1024];
    printf("\n-- Scores for '%s' (desktop=%d) --\n", query, mock_desktop);
    for (int i = 0; i < app->history_count; i++) {
        compose_display_string(&app->history[i], display, sizeof(display));
        score_t s = match_window(query, &app->history[i]);
        if (s > SCORE_MIN) {
            int wb = (app->history[i].desktop == mock_desktop && app->history[i].desktop != -1) ? 1 : 0;
            printf("  %8.0f  id=0x%02lx  d%d  '%s'\n",
                   s + wb, (unsigned long)app->history[i].id, app->history[i].desktop, display);
        }
    }
}

/* ------------------------------------------------------------------ */
/* RULE 4 — word-start: query matches contiguously at word boundary   */
/* ------------------------------------------------------------------ */

/*
 * "ch" — both Teams "Chat" and every Google Chrome window are TIER_DIRECT
 * ("ch" matches "Chrome" at a word boundary).  Teams must rank #1 because
 * "Chat" appears at the START of the title (title-relative offset 0),
 * while "Chrome" lands near the END of each Chrome window title.
 *
 * BASELINE: FAILS — workspace bonus floats d0 Chrome rows above Teams (d6).
 * AFTER Signal A (title-relative position bonus): must PASS.
 */
static void test_ch_ranks_chat(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    print_scores(&app, "ch");
    filter_windows(&app, "ch");
    ASSERT_RANK1("ch → Chat|Teams #1", &app, WIN_TEAMS);
}

/*
 * "chat" — same structure as "ch" but more letters → Chat at title start
 * is a longer contiguous match.  Must beat "Google Chrome" occurrences.
 */
static void test_chat_ranks_chat(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "chat");
    ASSERT_RANK1("chat → Chat|Teams #1", &app, WIN_TEAMS);
}

/* ------------------------------------------------------------------ */
/* Distinctive-token cases (RULE 3/4 — unambiguous single match)      */
/* ------------------------------------------------------------------ */

static void test_tea_ranks_teams(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "tea");
    ASSERT_RANK1("tea → Teams #1", &app, WIN_TEAMS);
}

static void test_slack(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "slack");
    ASSERT_RANK1("slack → Slack window #1", &app, WIN_SLACK);
}

static void test_kraken(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "kraken");
    ASSERT_RANK1("kraken → Kraken Pro #1", &app, WIN_KRAKEN);
}

static void test_tsunami(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "tsunami");
    ASSERT_RANK1("tsunami → Thunderbird #1", &app, WIN_TSUNAMI);
}

static void test_zcrew(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "zcrew");
    ASSERT_RANK1("zcrew → zcrew|main #1", &app, WIN_ZCREW);
}

static void test_specdd(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "specdd");
    ASSERT_RANK1("specdd → specdd/specdd #1", &app, WIN_SPECDD);
}

static void test_biokat(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "biokat");
    ASSERT_RANK1("biokat → biokat window #1", &app, WIN_BIOKAT);
}

static void test_sika(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "sika");
    ASSERT_RANK1("sika → SIKA Teichfolie #1", &app, WIN_SIKA_TEICH);
}

static void test_teich(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "teich");
    ASSERT_RANK1("teich → SIKA Teichfolie #1", &app, WIN_SIKA_TEICH);
}

/*
 * "yt" — both YouTube windows must appear in top 2.
 * "yt" matches: Y(ouTube) is a word-start, t(ipping or Tube?) — actually
 * "yt" matches "YouTube" (y at word start after '-', t consecutive).
 * Both YouTube windows contain "YouTube - Google Chrome".
 */
static void test_yt_top2(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "yt");
    ASSERT_IN_TOP2("yt → both YouTube windows in top 2",
                   &app, WIN_YT_ANTHROPIC, WIN_YT_SOFTENG);
}

/*
 * "cofi" — cofi|main terminal and Cofi Issues chrome tab.
 * Both contain the word "cofi"/"Cofi" at a word boundary.
 */
static void test_cofi_top2(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "cofi");
    ASSERT_IN_TOP2("cofi → cofi|main + Cofi Issues in top 2",
                   &app, WIN_COFI_TERM, WIN_COFI_ISSUES);
}

/*
 * "anthropic" — distinctive token, only Anthropic-YouTube window.
 */
static void test_anthropic(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "anthropic");
    ASSERT_RANK1("anthropic → Anthropic YouTube #1", &app, WIN_YT_ANTHROPIC);
}

/*
 * "kuppel" — distinctive token for the kuppelfenster window.
 */
static void test_kuppel(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "kuppel");
    ASSERT_RANK1("kuppel → kuppelfenster #1", &app, WIN_KUPPEL);
}

/*
 * "coiner" — both coiner terminals in top 2.
 */
static void test_coiner_top2(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "coiner");
    ASSERT_IN_TOP2("coiner → both coiner terminals in top 2",
                   &app, WIN_COINER_TERM1, WIN_COINER_TERM2);
}

/*
 * "hyperliquid" — distinctive token for Kraken window.
 */
static void test_hyperliquid(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "hyperliquid");
    ASSERT_RANK1("hyperliquid → Kraken Pro #1", &app, WIN_KRAKEN);
}

/* ------------------------------------------------------------------ */
/* RULE 5 — consecutive word-starts across fields                     */
/* ------------------------------------------------------------------ */

/*
 * "gcse" — g(oogle-chrome) c(hrome) S(oftware) e(ngineering).
 * The Software-engineering-YouTube window has g·c as consecutive
 * word-starts (google-chrome), and s·e as consecutive word-starts
 * (Software engineering).  Two consecutive pairs vs. scattered matches
 * in other windows.  Must rank #1.
 *
 * BASELINE: may fail if fzf alone cannot distinguish consecutive-word-start
 * density.  AFTER Signal B (consecutive word-start bonus): must PASS.
 */
static void test_gcse_ranks_softeng(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    print_scores(&app, "gcse");
    filter_windows(&app, "gcse");
    ASSERT_RANK1("gcse → Software engineering YouTube #1", &app, WIN_YT_SOFTENG);
}

/* ------------------------------------------------------------------ */
/* Bug 1: TIER invariant — indirect score must never reach TIER_DIRECT */
/* ------------------------------------------------------------------ */

/*
 * Pathological indirect case: instance/title/class filled with single-char
 * words so that 323 filter chars each match at a word-start boundary.  The
 * match is TIER_INDIRECT (chars separated by spaces, no contiguous run).
 *
 * Without the INDIRECT_SCORE_MAX clamp, fzf + Signal-B ≈ 7442 + 2576 =
 * 10018 > TIER_DIRECT_BASE (10000).  With the clamp the score is capped at
 * 9999 < TIER_DIRECT_BASE.
 */
static void test_tier_invariant(void) {
    AppData app;
    reset_app(&app);

    char instance[MAX_CLASS_LEN];
    char title[MAX_TITLE_LEN];
    char class_nm[MAX_CLASS_LEN];
    char filter[512];
    int n = 0;

    /* 63 single-char words in instance: "a b c ... k" (125 chars) */
    int ipos = 0;
    for (int i = 0; i < 63; i++) {
        char c = (char)('a' + (i % 26));
        if (i > 0 && ipos < (int)sizeof(instance) - 2) instance[ipos++] = ' ';
        if (ipos < (int)sizeof(instance) - 1)           instance[ipos++] = c;
        if (n  < (int)sizeof(filter) - 1)               filter[n++] = c;
    }
    instance[ipos] = '\0';

    /* 197 single-char words in title: "A B C ... V" (393 chars) */
    int tpos = 0;
    for (int i = 0; i < 197; i++) {
        char c = (char)('A' + (i % 26));
        if (i > 0 && tpos < (int)sizeof(title) - 2) title[tpos++] = ' ';
        if (tpos < (int)sizeof(title) - 1)           title[tpos++] = c;
        if (n  < (int)sizeof(filter) - 1)            filter[n++] = (char)(c | 32);
    }
    title[tpos] = '\0';

    /* 63 single-char words in class: "a b c ... k" (125 chars) */
    int cpos = 0;
    for (int i = 0; i < 63; i++) {
        char c = (char)('a' + (i % 26));
        if (i > 0 && cpos < (int)sizeof(class_nm) - 2) class_nm[cpos++] = ' ';
        if (cpos < (int)sizeof(class_nm) - 1)           class_nm[cpos++] = c;
        if (n  < (int)sizeof(filter) - 1)               filter[n++] = c;
    }
    class_nm[cpos] = '\0';
    filter[n] = '\0';

    add_win(&app, 0x100, 0, instance, title, class_nm);

    score_t raw = match_window(filter, &app.history[0]);
    printf("  tier invariant: %d-char filter, raw indirect=%.0f (tier_base=%d indirect_max=%d)\n",
           n, raw, TIER_DIRECT_BASE, INDIRECT_SCORE_MAX);

    ASSERT_TRUE("tier invariant: raw indirect score < TIER_DIRECT_BASE",
                raw > SCORE_MIN && raw < (score_t)TIER_DIRECT_BASE);

    /* workspace_bonus=1 is added after match_window in score_and_filter_windows;
     * INDIRECT_SCORE_MAX must leave room for it so the final score stays strictly
     * below the tier boundary.  INDIRECT_SCORE_MAX must be <= TIER_DIRECT_BASE - 2. */
    ASSERT_TRUE("tier invariant: final indirect score (raw + workspace_bonus) < TIER_DIRECT_BASE",
                raw > SCORE_MIN && raw + 1 < (score_t)TIER_DIRECT_BASE);
}

/* ------------------------------------------------------------------ */
/* Bug 1: direct matches must NOT be clamped to INDIRECT_SCORE_MAX    */
/* ------------------------------------------------------------------ */

/*
 * A 500-char direct match has fzf ≈ 36 + 499*20 = 10016 > INDIRECT_SCORE_MAX.
 * With the clamp wrongly placed BEFORE the TIER_DIRECT check, the direct score
 * is silently capped before +TIER_DIRECT_BASE, causing two long direct matches
 * to collapse to the same total.  With the clamp correctly placed on the INDIRECT
 * path only, the direct match keeps its full fzf and scores above
 * TIER_DIRECT_BASE + INDIRECT_SCORE_MAX.
 */
static void test_direct_match_not_clamped(void) {
    AppData app;
    reset_app(&app);

    char filter[502];
    char title[MAX_TITLE_LEN];
    for (int i = 0; i < 500; i++) {
        filter[i] = (char)('a' + (i % 26));
        title[i]  = (char)('a' + (i % 26));
    }
    filter[500] = '\0';
    title[500]  = '\0';

    add_win(&app, 0x100, 0, "app", title, "app");
    score_t s = match_window(filter, &app.history[0]);
    printf("  500-char direct match score: %.0f  (tier_base=%d indirect_max=%d)\n",
           s, TIER_DIRECT_BASE, INDIRECT_SCORE_MAX);

    /* If the clamp were applied to this direct match the score would be exactly
     * TIER_DIRECT_BASE + INDIRECT_SCORE_MAX (the clamp cap + the tier offset).
     * With the clamp correctly restricted to the indirect path, the score is
     * strictly higher (full fzf ≈ 10016, not capped). */
    /* The title starts with the filter at offset 0, so Signal A = MATCH_EARLY_BONUS_MAX.
     * With the wrong clamp placement the score is exactly:
     *   INDIRECT_SCORE_MAX + TIER_DIRECT_BASE + MATCH_EARLY_BONUS_MAX
     * With the clamp correctly on the indirect path, fzf is unclamped (≈10016)
     * and the score is strictly higher than that ceiling. */
    ASSERT_TRUE("direct not clamped: score > INDIRECT_SCORE_MAX + TIER_DIRECT_BASE + MATCH_EARLY_BONUS_MAX",
                s > (score_t)(INDIRECT_SCORE_MAX + TIER_DIRECT_BASE + MATCH_EARLY_BONUS_MAX));
}

/*
 * Two TIER_DIRECT matches for the same short query with different Signal A
 * (title-relative position) bonuses.  Guards that direct-vs-direct ordering
 * is never collapsed by the clamp (even when fzf is identical, Signal A
 * differentiates them).
 */
static void test_direct_match_ordering(void) {
    AppData app;
    reset_app(&app);
    mock_desktop = 0;

    add_win(&app, 0x100, 0, "app", "brew | direct-a", "app"); /* "brew" at title offset 0 */
    add_win(&app, 0x200, 0, "app", "xxx brew direct-b", "app"); /* "brew" at title offset 4 */

    filter_windows(&app, "brew");
    ASSERT_RANK1("direct ordering: earlier title position ranks first", &app, 0x100);
}

/* ------------------------------------------------------------------ */
/* Bug 2: Signal B best-alignment (greedy undercounts repeated letters) */
/* ------------------------------------------------------------------ */

/*
 * "aba" on display "[1] app X A B A app":
 *   Greedy: a(pos4) → b(pos12) → a(pos14).  Adjacency: (4,12) NOT adjacent
 *   (X at pos8 is a word-start between them) → 1 pair.
 *   DP best: a(pos10) → b(pos12) → a(pos14).  Both pairs adjacent → 2 pairs.
 */
static void test_aba_best_alignment(void) {
    AppData app;
    reset_app(&app);

    add_win(&app, 0x100, 0, "app", "X A B A", "app");

    char display[1024];
    compose_display_string(&app.history[0], display, sizeof(display));
    int pairs = consecutive_word_start_pairs("aba", display);
    printf("  aba on '%s': pairs=%d (want 2)\n", display, pairs);

    ASSERT_TRUE("aba best-alignment: 2 adjacent word-start pairs", pairs == 2);
}

/* ------------------------------------------------------------------ */
/* Negative / top-N guards — spurious promotion checks                */
/* ------------------------------------------------------------------ */

/*
 * "chat" has a genuine #1 winner (Teams: "Chat" at title offset 0).
 * specdd title has "that enables" — 't','h' match, plus scattered 'a'.
 * A weak scattered / cross-field match must not displace the real hits.
 */
static void test_chat_no_specdd_top2(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "chat");
    ASSERT_NOT_IN_TOP2("chat: specdd not in top 2", &app, WIN_SPECDD);
}

/*
 * "tea" — Teams has "Tea" at the very start of "Teams" (title offset 0).
 * specdd "that enables agents" has t-e-a scattered but should stay below
 * the genuine word-boundary hit.
 */
static void test_tea_no_specdd_top2(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "tea");
    ASSERT_NOT_IN_TOP2("tea: specdd not in top 2", &app, WIN_SPECDD);
}

/*
 * "ch" — winner is Teams ("Chat" at title offset 0).
 * Kraken's display string "google-chrome ... Hyperliquid ... Kraken Pro"
 * has 'c' (chrome) and 'H' (Hyperliquid) as adjacent word-starts, giving
 * it a spurious Signal-B boost.  It must not jump into top 2 ahead of
 * genuine word-boundary matches (Teams + Cofi Issues both have "Ch").
 */
static void test_ch_no_kraken_top2(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "ch");
    ASSERT_NOT_IN_TOP2("ch: Kraken not in top 2", &app, WIN_KRAKEN);
}

/*
 * "gcse" — Software Engineering YouTube (#1) plus specdd as the second-best
 * legitimate acronym match (g·c from google-chrome, s·e from Specification/
 * Development).  Confirms that a genuine second-best acronym IS in top 2
 * so future scoring tightening doesn't silently drop it.
 */
static void test_gcse_softeng_specdd_top2(void) {
    AppData app;
    load_fixture(&app);
    mock_desktop = 0;
    filter_windows(&app, "gcse");
    ASSERT_IN_TOP2("gcse: softeng + specdd in top 2", &app, WIN_YT_SOFTENG, WIN_SPECDD);
}

/* ---- Main ---- */

int main(void) {
    log_set_quiet(true);

    /* RULE 4 — word-start ranking (title-relative position) */
    test_ch_ranks_chat();
    test_chat_ranks_chat();
    test_tea_ranks_teams();

    /* Distinctive tokens (rules 3/4, unambiguous) */
    test_slack();
    test_kraken();
    test_tsunami();
    test_zcrew();
    test_specdd();
    test_biokat();
    test_sika();
    test_teich();
    test_yt_top2();
    test_cofi_top2();
    test_anthropic();
    test_kuppel();
    test_coiner_top2();
    test_hyperliquid();

    /* RULE 5 — consecutive word-start density */
    test_gcse_ranks_softeng();

    /* Negative guards — spurious top-2 promotions */
    test_chat_no_specdd_top2();
    test_tea_no_specdd_top2();
    test_ch_no_kraken_top2();
    test_gcse_softeng_specdd_top2();

    /* Bug hardening — tier invariant + Signal B best-alignment */
    test_tier_invariant();
    test_aba_best_alignment();
    test_direct_match_not_clamped();
    test_direct_match_ordering();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return (fail == 0) ? 0 : 1;
}
