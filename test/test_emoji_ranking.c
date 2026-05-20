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

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
