#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static void reset_history_state(void) {
    free(s_history_glyphs);
    free(s_history_indices);
    s_history_glyphs = NULL;
    s_history_indices = NULL;
    s_history_count = 0;
    s_history_capacity = 0;
    s_history_loaded = 0;
}

static void setup_home(const char *name) {
    char home[256];
    snprintf(home, sizeof(home), "/tmp/%s", name);
    mkdir(home, 0755);
    setenv("HOME", home, 1);
    reset_history_state();
}

static int rank_of_glyph(AppData *app, const char *glyph) {
    for (int i = 0; i < app->filtered_emoji_count; i++) {
        int idx = app->filtered_emoji[i];
        if (strcmp(EMOJI_TABLE[idx].glyph, glyph) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_match_pair_for_t1(const char **query_out, int *strong_idx_out, int *weak_idx_out) {
    static const char *queries[] = {"fire", "joy", "heart", "rocket", "flag", "cat", "face", "arrow", "up"};
    for (size_t qi = 0; qi < sizeof(queries) / sizeof(queries[0]); qi++) {
        const char *q = queries[qi];
        int strong = -1;
        int weak = -1;
        for (int i = 0; i < EMOJI_TABLE_LEN; i++) {
            int score = emoji_rank_score(q, &EMOJI_TABLE[i]);
            if (score <= 0) {
                continue;
            }
            if (name_has_word(EMOJI_TABLE[i].name_norm, q)) {
                if (strong < 0) strong = i;
            } else {
                if (weak < 0) weak = i;
            }
            if (strong >= 0 && weak >= 0) {
                *query_out = q;
                *strong_idx_out = strong;
                *weak_idx_out = weak;
                return 1;
            }
        }
    }
    return 0;
}

static void test_t1_strong_name_match_beats_heavy_mru_weak_match(void) {
    setup_home("cofi-emoji-mru-t1");
    const char *query = NULL;
    int strong_idx = -1;
    int weak_idx = -1;
    int found = find_match_pair_for_t1(&query, &strong_idx, &weak_idx);
    ASSERT_TRUE("T1: found query with strong name match and weak keyword match", found);
    if (!found) return;

    for (int i = 0; i < 200; i++) {
        emoji_history_push(EMOJI_TABLE[weak_idx].glyph);
    }

    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, query);
    ASSERT_TRUE("T1: query has matches", app.filtered_emoji_count > 0);
    int strong_rank = rank_of_glyph(&app, EMOJI_TABLE[strong_idx].glyph);
    int weak_rank = rank_of_glyph(&app, EMOJI_TABLE[weak_idx].glyph);
    ASSERT_TRUE("T1: strong name match present", strong_rank >= 0);
    ASSERT_TRUE("T1: weak MRU-biased match present", weak_rank >= 0);
    ASSERT_TRUE("T1: strong name match still ranks above weak heavy-MRU match", strong_rank < weak_rank);
}

static void test_t2_mru_breaks_ties(void) {
    setup_home("cofi-emoji-mru-t2");
    AppData app;
    memset(&app, 0, sizeof(app));

    int a = -1;
    int b = -1;
    for (int i = 0; i < EMOJI_TABLE_LEN; i++) {
        int si = emoji_rank_score("face", &EMOJI_TABLE[i]);
        if (si <= 0) continue;
        for (int j = i + 1; j < EMOJI_TABLE_LEN; j++) {
            int sj = emoji_rank_score("face", &EMOJI_TABLE[j]);
            if (sj == si) {
                a = i;
                b = j;
                break;
            }
        }
        if (a >= 0) break;
    }

    ASSERT_TRUE("T2: found equal-score face pair", a >= 0 && b >= 0);
    if (a < 0 || b < 0) return;

    emoji_on_query_changed(&app, "face");
    int rank_a_before = rank_of_glyph(&app, EMOJI_TABLE[a].glyph);
    int rank_b_before = rank_of_glyph(&app, EMOJI_TABLE[b].glyph);
    ASSERT_TRUE("T2: pair present before MRU", rank_a_before >= 0 && rank_b_before >= 0);

    int mru_idx = rank_a_before > rank_b_before ? a : b;
    int other_idx = mru_idx == a ? b : a;
    emoji_history_push(EMOJI_TABLE[mru_idx].glyph);
    emoji_on_query_changed(&app, "face");

    int rank_mru = rank_of_glyph(&app, EMOJI_TABLE[mru_idx].glyph);
    int rank_other = rank_of_glyph(&app, EMOJI_TABLE[other_idx].glyph);
    ASSERT_TRUE("T2: MRU item ranks above equal-score peer", rank_mru >= 0 && rank_other >= 0 && rank_mru < rank_other);
}

static void test_t3_mru_never_resurrects_or_beats_strong(void) {
    setup_home("cofi-emoji-mru-t3");
    emoji_history_push("😂");

    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "fire");
    ASSERT_TRUE("T3: strong fire match stays above MRU noise", rank_of_glyph(&app, "🔥") == 0);

    emoji_on_query_changed(&app, "zzzzzzzz");
    ASSERT_TRUE("T3: non-match query returns zero rows", app.filtered_emoji_count == 0);
}

static void test_empty_query_unchanged_by_history(void) {
    setup_home("cofi-emoji-mru-empty");
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "");

    int baseline[8];
    for (int i = 0; i < 8; i++) baseline[i] = app.filtered_emoji[i];

    emoji_history_push("😂");
    emoji_history_push("🔥");
    emoji_history_push("😄");
    emoji_on_query_changed(&app, "");

    int unchanged = 1;
    for (int i = 0; i < 8; i++) {
        if (app.filtered_emoji[i] != baseline[i]) {
            unchanged = 0;
            break;
        }
    }
    ASSERT_TRUE("empty query order unchanged by MRU", unchanged);
}

static void test_t5_dedup_move_to_front(void) {
    setup_home("cofi-emoji-mru-t5");
    emoji_history_push("😂");
    emoji_history_push("🔥");
    emoji_history_push("😂");
    ASSERT_TRUE("T5: dedup keeps count unchanged", s_history_count == 2);
    ASSERT_TRUE("T5: repeated glyph moved to front", strcmp(s_history_glyphs[0], "😂") == 0);
}

static void test_t7_persistence_round_trip(void) {
    setup_home("cofi-emoji-mru-t7");
    emoji_history_push("😂");
    emoji_history_push("🔥");
    emoji_history_save();

    reset_history_state();
    emoji_history_load();
    ASSERT_TRUE("T7: history count restored", s_history_count == 2);
    ASSERT_TRUE("T7: most recent restored first", strcmp(s_history_glyphs[0], "🔥") == 0);
    ASSERT_TRUE("T7: second restored", strcmp(s_history_glyphs[1], "😂") == 0);
}

static void test_long_glyph_round_trip_and_reindex(void) {
    setup_home("cofi-emoji-mru-long-glyph");
    int idx = -1;
    for (int i = 0; i < EMOJI_TABLE_LEN; i++) {
        if (strlen(EMOJI_TABLE[i].glyph) > 15) {
            idx = i;
            break;
        }
    }
    ASSERT_TRUE("long glyph: found >15-byte glyph in table", idx >= 0);
    if (idx < 0) return;

    const char *glyph = EMOJI_TABLE[idx].glyph;
    emoji_history_push(glyph);
    ASSERT_TRUE("long glyph: stored intact", strcmp(s_history_glyphs[0], glyph) == 0);
    ASSERT_TRUE("long glyph: reindexed to table", s_history_indices[0] == idx);

    emoji_history_save();
    reset_history_state();
    emoji_history_load();
    ASSERT_TRUE("long glyph: round-trip stored intact", strcmp(s_history_glyphs[0], glyph) == 0);
    ASSERT_TRUE("long glyph: round-trip reindex works", s_history_indices[0] == idx);

    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, EMOJI_TABLE[idx].name_norm);
    int rank = rank_of_glyph(&app, glyph);
    ASSERT_TRUE("long glyph: appears in filtered results", rank >= 0);
}

static void test_t8_load_idempotent(void) {
    setup_home("cofi-emoji-mru-t8");
    FILE *f = fopen(emoji_history_path(), "w");
    ASSERT_TRUE("T8: opened history file", f != NULL);
    if (!f) return;
    fprintf(f, "{\n  \"picks\": [\n    {\"glyph\": \"😂\"}\n  ]\n}\n");
    fclose(f);

    emoji_history_load();
    ASSERT_TRUE("T8: loaded count once", s_history_count == 1);
    strcpy(s_history_glyphs[0], "X");
    emoji_history_load();
    ASSERT_TRUE("T8: second load is no-op", strcmp(s_history_glyphs[0], "X") == 0);
}

static void test_u1_unicode_multibyte_roundtrip(void) {
    setup_home("cofi-emoji-mru-u1");
    /* Multi-codepoint glyphs: woman shrugging (U+1F937 U+200D U+2640 U+FE0F)
     * and family (U+1F9D1 U+200D U+1F91D U+200D U+1F9D1) */
    const char *glyphs[] = {
        "🤷‍♀️",  /* woman shrugging: f0 9f a4 b7 e2 80 8d e2 99 80 ef b8 8f (13 bytes) */
        "😀",     /* grinning face: f0 9f 98 80 (4 bytes) */
        "🦐",     /* shrimp: f0 9f a6 90 (4 bytes) */
        "⬆️",     /* up arrow + variation selector: e2 ac 86 ef b8 8f (7 bytes) */
        "👨‍👩‍👦",   /* family: f0 9f 91 a8 e2 80 8d f0 9f 91 a9 e2 80 8d f0 9f 91 a6 (25 bytes) */
        NULL
    };

    for (int i = 0; glyphs[i]; i++) {
        emoji_history_push(glyphs[i]);
    }

    emoji_history_save();

    /* Snapshot glyphs before reset */
    char saved[5][EMOJI_GLYPH_MAX_BYTES];
    int saved_count = s_history_count;
    for (int i = 0; i < saved_count && i < 5; i++) {
        memcpy(saved[i], s_history_glyphs[i], EMOJI_GLYPH_MAX_BYTES);
    }

    reset_history_state();
    emoji_history_load();

    ASSERT_TRUE("U1: glyph count restored after roundtrip", s_history_count == saved_count);
    for (int i = 0; i < saved_count; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "U1: glyph[%d] roundtrip byte-equal", i);
        ASSERT_TRUE(msg, strcmp(s_history_glyphs[i], saved[i]) == 0);
    }
}

static void test_u2_zwj_glyph_save_load_exact(void) {
    setup_home("cofi-emoji-mru-u2");
    /* Woman shrugging — the most complex emoji: ZWJ + variation selector */
    const char *shrug = "🤷‍♀️";
    emoji_history_push(shrug);
    ASSERT_TRUE("U2: shrug stored as pushed",
                strcmp(s_history_glyphs[0], shrug) == 0);
    ASSERT_TRUE("U2: shrug byte length is 13 (with ZWJ+VS16)",
                strlen(s_history_glyphs[0]) == 13);

    emoji_history_save();

    /* Read back the raw file and verify the glyph appears as-encoded */
    FILE *f = fopen(emoji_history_path(), "r");
    ASSERT_TRUE("U2: history file exists after save", f != NULL);
    if (!f) return;

    char buf[2048];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    ASSERT_TRUE("U2: shrug glyph literal present in saved JSON",
                strstr(buf, shrug) != NULL);

    reset_history_state();
    emoji_history_load();
    ASSERT_TRUE("U2: shrug survives load after reset",
                s_history_count == 1 &&
                strcmp(s_history_glyphs[0], shrug) == 0);
}

static void test_t9_unknown_glyph_round_trip(void) {
    setup_home("cofi-emoji-mru-t9");
    FILE *f = fopen(emoji_history_path(), "w");
    ASSERT_TRUE("T9: opened history file", f != NULL);
    if (!f) return;
    fprintf(f, "{\n  \"picks\": [\n    {\"glyph\": \"NOTREAL\"},\n    {\"glyph\": \"😂\"}\n  ]\n}\n");
    fclose(f);

    emoji_history_load();
    ASSERT_TRUE("T9: loaded unknown glyph entry", s_history_count == 2);
    ASSERT_TRUE("T9: unknown glyph retained", strcmp(s_history_glyphs[0], "NOTREAL") == 0);
    ASSERT_TRUE("T9: unknown glyph index is -1", s_history_indices[0] == -1);
    ASSERT_TRUE("T9: known glyph still indexed", s_history_indices[1] >= 0);
}


int main(void) {
    printf("Emoji MRU history tests\n");
    printf("=======================\n\n");

    test_t1_strong_name_match_beats_heavy_mru_weak_match();
    test_t2_mru_breaks_ties();
    test_t3_mru_never_resurrects_or_beats_strong();
    test_empty_query_unchanged_by_history();
    test_t5_dedup_move_to_front();
    test_t7_persistence_round_trip();
    test_long_glyph_round_trip_and_reindex();
    test_t8_load_idempotent();
    test_t9_unknown_glyph_round_trip();
    test_u1_unicode_multibyte_roundtrip();
    test_u2_zwj_glyph_save_load_exact();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
