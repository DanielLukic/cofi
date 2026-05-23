#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/command_registry.h"
#include "../src/cofi_tab_provider.h"
#include "../src/emoji_data.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        } \
    } while (0)

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    (void)p;
    return 0;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    (void)provider_id;
    return NULL;
}

int cofi_register_command(const CommandSpec *spec) {
    (void)spec;
    return 0;
}

void exit_command_mode(AppData *app) {
    (void)app;
}

void surface_tab(AppData *app, TabMode tab) {
    (void)app;
    (void)tab;
}

#include "../src/emoji_provider.c"

static int rank_of_glyph(AppData *app, const char *glyph) {
    for (int i = 0; i < app->filtered_emoji_count; i++) {
        int idx = app->filtered_emoji[i];
        if (strcmp(EMOJI_TABLE[idx].glyph, glyph) == 0) {
            return i;
        }
    }
    return -1;
}

static void assert_top1(const char *query, const char *glyph) {
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, query);
    ASSERT_TRUE("query returns at least one row", app.filtered_emoji_count > 0);
    int rank = rank_of_glyph(&app, glyph);
    ASSERT_TRUE("expected glyph present", rank >= 0);
    ASSERT_TRUE("expected glyph is top result", rank == 0);
}

static void assert_top3(const char *query, const char *glyph) {
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, query);
    ASSERT_TRUE("query returns at least one row", app.filtered_emoji_count > 0);
    int rank = rank_of_glyph(&app, glyph);
    ASSERT_TRUE("expected glyph present", rank >= 0);
    ASSERT_TRUE("expected glyph is within top 3", rank >= 0 && rank < 3);
}

static void assert_ranked_above(const char *query, const char *better_glyph, const char *worse_glyph) {
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, query);
    int better = rank_of_glyph(&app, better_glyph);
    int worse = rank_of_glyph(&app, worse_glyph);
    ASSERT_TRUE("better glyph present", better >= 0);
    ASSERT_TRUE("worse glyph present", worse >= 0);
    ASSERT_TRUE("better glyph ranks above worse glyph", better < worse);
}

static void assert_not_in_top_n(const char *query, const char *glyph, int n) {
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, query);
    int rank = rank_of_glyph(&app, glyph);
    ASSERT_TRUE("unexpected glyph not in top N", rank < 0 || rank >= n);
}

// Reset in-memory MRU state without touching disk.
// Needed to isolate MRU-sensitive ranking tests.
static void reset_mru_state(void) {
    free(s_history_glyphs);
    free(s_history_indices);
    s_history_glyphs = NULL;
    s_history_indices = NULL;
    s_history_count = 0;
    s_history_capacity = 0;
    s_history_loaded = 1;  // Suppress disk load for subsequent calls
}

// BUG REPRO: "sl" → sleeping face (😴, name_norm starts with "sl") must rank above
// grinning face (😀, only matches via keywords "smile").  Without MRU this holds.
// With grinning at max-MRU the bug fires: keyword_score(96)+MRU(5000)=5096 beats
// name_prefix_score(496)+PREFIX_BOOST(600)=1096.  Test 1 passes; Test 2 fails today.
static void test_sl_name_prefix_beats_keywords_no_mru(void) {
    reset_mru_state();
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "sl");
    int sleeping = rank_of_glyph(&app, "😴");
    int grinning = rank_of_glyph(&app, "😀");
    ASSERT_TRUE("sl/no-mru: sleeping face present (name prefix)", sleeping >= 0);
    ASSERT_TRUE("sl/no-mru: grinning face present (keywords via smile)", grinning >= 0);
    ASSERT_TRUE("sl/no-mru: name-prefix (sleeping) ranks above keywords-only (grinning)", sleeping < grinning);
}

// FAILING TEST — demonstrates MRU cross-tier bug (#47).
// grinning face has max MRU (position 0, +5000 bonus); sleeping face has none.
// Expected: sleeping still wins (name-prefix tier > keywords tier).
// Actual:   grinning wins (MRU bonus crosses tier boundary).
static void test_sl_name_prefix_beats_keywords_max_mru(void) {
    reset_mru_state();
    emoji_history_push("😀");  // grinning face → position 0, +EMOJI_MRU_MAX_BONUS (5000)
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "sl");
    int sleeping = rank_of_glyph(&app, "😴");
    int grinning = rank_of_glyph(&app, "😀");
    ASSERT_TRUE("sl/max-mru: sleeping face present", sleeping >= 0);
    ASSERT_TRUE("sl/max-mru: grinning face present", grinning >= 0);
    // Currently FAILS: MRU bonus (5000) >> name-prefix boost (600) + fzf gap
    ASSERT_TRUE("sl/max-mru: name-prefix (sleeping) beats keywords-only+max-MRU (grinning)",
                sleeping < grinning);
}

// Acronym tier: "sf" → sleeping face has consecutive word-start pairs (s→sleeping,
// f→face). Surfer (🏄) only matches "sf" mid-word in "surfing" — no word-start 'f'
// — so it stays at tier 0. After tier redesign: sleeping face (tier 1) ranks above.
static void test_acronym_tier_sf_beats_non_structural(void) {
    reset_mru_state();
    assert_ranked_above("sf", "😴", "🏄");
}

// BUG A repro: query "sf" — both 😴 (sleeping face) and 😤 (face with steam from
// nose) are tier-1 acronym matches (1 consecutive word-start pair each). The
// intra-tier score is pure fzf, which favours 😤 due to shorter gap between 's'
// and 'f' positions. UX expects the tighter/shorter name (😴) to win.
static void test_acronym_tier_sf_sleeping_beats_steam_from_nose(void) {
    reset_mru_state();
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "sf");
    int sleeping = rank_of_glyph(&app, "😴");
    int steam    = rank_of_glyph(&app, "😤");
    ASSERT_TRUE("sf/bug-a: sleeping face present", sleeping >= 0);
    ASSERT_TRUE("sf/bug-a: face-with-steam present", steam >= 0);
    // Diagnostic dump: print both rank scores so the failure log is actionable.
    int s_score = emoji_rank_score("sf", &EMOJI_TABLE[app.filtered_emoji[sleeping]]);
    int t_score = emoji_rank_score("sf", &EMOJI_TABLE[app.filtered_emoji[steam]]);
    printf("    sf/diag: sleeping rank=%d score=%d ; steam rank=%d score=%d\n",
           sleeping, s_score, steam, t_score);
    ASSERT_TRUE("sf/bug-a: sleeping face beats face-with-steam-from-nose",
                sleeping < steam);
}

// BUG B: MRU same-tier promotion. "aup" → both ⬆️ (up arrow) and ⤴️ (right
// arrow curving up) match. With ⬆️ in MRU at recency 0, it should rank #1.
// Same-tier promotion: bucket-promote MRU'd items within the top tier only.
static void test_mru_promotes_same_tier_within_top_tier(void) {
    reset_mru_state();
    emoji_history_push("⬆️");
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "aup");
    int up = rank_of_glyph(&app, "⬆️");
    int curving_up = rank_of_glyph(&app, "⤴️");
    ASSERT_TRUE("aup/mru: up arrow present", up >= 0);
    ASSERT_TRUE("aup/mru: right arrow curving up present", curving_up >= 0);
    // Verify both candidates inhabit the SAME tier so this exercises
    // same-tier promotion (not a cross-tier accident).
    EmojiRank up_rank = emoji_rank_score_full("aup", &EMOJI_TABLE[app.filtered_emoji[up]]);
    EmojiRank cu_rank = emoji_rank_score_full("aup", &EMOJI_TABLE[app.filtered_emoji[curving_up]]);
    printf("    aup/diag: up tier=%d score=%d ; curving_up tier=%d score=%d\n",
           up_rank.tier_id, up_rank.total, cu_rank.tier_id, cu_rank.total);
    ASSERT_TRUE("aup/mru: up and curving-up share a tier",
                up_rank.tier_id == cu_rank.tier_id);
    ASSERT_TRUE("aup/mru: MRU-promoted up arrow ranks #1", up == 0);
}

// Zone-boundary guard: MRU must NEVER cross the junk→structural boundary.
// 😀 (grinning) is in MRU at max recency and matches "sl" only via keyword
// (tier 0 = junk zone). 😴 (sleeping face) has tier-3 name prefix
// (structural zone). Sleeping must still win — MRU promotion is allowed to
// reorder freely within the structural zone, but never lifts a junk-zone
// candidate above a structural one.
static void test_mru_does_not_cross_tiers(void) {
    reset_mru_state();
    emoji_history_push("😀");
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "sl");
    int sleeping = rank_of_glyph(&app, "😴");
    int grinning = rank_of_glyph(&app, "😀");
    ASSERT_TRUE("sl/cross-tier: sleeping face present", sleeping >= 0);
    ASSERT_TRUE("sl/cross-tier: grinning face present", grinning >= 0);
    ASSERT_TRUE("sl/cross-tier: tier-3 name-prefix beats MRU'd tier-0 keyword",
                sleeping < grinning);
}

// Zone-based MRU: cross-tier promotion within the structural zone. "shr"
// matches 🤷 person-shrugging at tier 2 (word prefix on "shrugging") and 🦐
// shrimp at tier 3 (name prefix). Without MRU, shrimp wins (higher tier).
// With shrug in MRU at recency 0, zone-based bucket promotion lifts shrug
// above shrimp — both are structural, MRU is allowed to reorder freely.
static void test_mru_promotes_across_structural_tiers_shr(void) {
    reset_mru_state();
    emoji_history_push("🤷");
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "shr");
    int shrug  = rank_of_glyph(&app, "🤷");
    int shrimp = rank_of_glyph(&app, "🦐");
    ASSERT_TRUE("shr/zone: person shrugging present", shrug >= 0);
    ASSERT_TRUE("shr/zone: shrimp present", shrimp >= 0);
    EmojiRank shrug_rank  = emoji_rank_score_full("shr", &EMOJI_TABLE[app.filtered_emoji[shrug]]);
    EmojiRank shrimp_rank = emoji_rank_score_full("shr", &EMOJI_TABLE[app.filtered_emoji[shrimp]]);
    printf("    shr/diag: shrug tier=%d score=%d ; shrimp tier=%d score=%d\n",
           shrug_rank.tier_id, shrug_rank.total, shrimp_rank.tier_id, shrimp_rank.total);
    ASSERT_TRUE("shr/zone: both candidates are structural (tier >= 1)",
                shrug_rank.tier_id >= 1 && shrimp_rank.tier_id >= 1);
    ASSERT_TRUE("shr/zone: shrug and shrimp inhabit different tiers (exercises cross-tier promotion)",
                shrug_rank.tier_id != shrimp_rank.tier_id);
    ASSERT_TRUE("shr/zone: MRU'd shrug (tier 2) ranks above shrimp (tier 3)",
                shrug < shrimp);
}

// Alias-exact tier (tier 6) must be invulnerable to a keyword-only+max-MRU attack.
// 😄 (smile) matches "joy" only via keywords; 😂 (joy) has alias "joy" → tier 6.
static void test_alias_exact_invulnerable_to_mru(void) {
    reset_mru_state();
    emoji_history_push("😄");  // smile: matches "joy" only via keyword
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "joy");
    int joy_face = rank_of_glyph(&app, "😂");
    int smile    = rank_of_glyph(&app, "😄");
    ASSERT_TRUE("alias-exact/mru: joy face (alias exact) present", joy_face >= 0);
    ASSERT_TRUE("alias-exact/mru: smile (keyword+mru) present", smile >= 0);
    ASSERT_TRUE("alias-exact/mru: alias-exact beats keyword+max-MRU", joy_face < smile);
}

int main(void) {
    printf("Emoji ranking oracle tests\n");
    printf("==========================\n\n");

    assert_top1("joy", "😂");
    assert_top1("heart", "❤️");
    assert_top1("fire", "🔥");
    assert_top1("rocket", "🚀");
    assert_top1("ROCKET", "🚀");
    assert_top1("rofl", "🤣");
    assert_top1("thumbsup", "👍");
    assert_top1("smile", "😄");
    assert_top1("100", "💯");
    assert_top3("aup", "⬆️");
    assert_top1("cat", "🐱");
    assert_top1("cat joy", "😹");
    assert_top1("joy cat", "😹");
    assert_top1("face joy", "😂");
    assert_top1("joy face", "😂");
    assert_top1("rócket", "🚀");
    assert_ranked_above("ro", "🚀", "🚣‍♀️");
    assert_ranked_above("ro", "🪨", "🧖‍♂️");
    assert_not_in_top_n("r", "🇸🇹", 5);

    test_sl_name_prefix_beats_keywords_no_mru();
    test_sl_name_prefix_beats_keywords_max_mru();
    test_acronym_tier_sf_beats_non_structural();
    test_acronym_tier_sf_sleeping_beats_steam_from_nose();
    test_mru_promotes_same_tier_within_top_tier();
    test_mru_does_not_cross_tiers();
    test_mru_promotes_across_structural_tiers_shr();
    test_alias_exact_invulnerable_to_mru();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
