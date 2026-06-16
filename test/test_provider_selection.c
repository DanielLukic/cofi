#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_registry.h"
#include "core/selection/selection.h"

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
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

void update_display(AppData *app) {
    (void)app;
}

int get_max_display_lines_dynamic(AppData *app) {
    (void)app;
    return 10;
}

const char *tab_log_name(TabMode tab) {
    (void)tab;
    return "provider";
}

void exit_command_mode(AppData *app) {
    (void)app;
}

void surface_tab(AppData *app, TabMode tab) {
    (void)app;
    (void)tab;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}

#include "providers/cofi_tab_provider.c"
#include "core/selection/selection.c"
#include "emoji/emoji_provider.c"

static int s_long_provider_id = -1;
static int s_reset_provider_id = -1;

static int long_provider_row_count(AppData *app) {
    (void)app;
    return 2;
}

static const char *long_provider_row_identity(AppData *app, int raw_idx) {
    (void)app;
    static char id0[PROVIDER_ID_MAX + 64];
    static int inited = 0;
    if (!inited) {
        memset(id0, 'x', PROVIDER_ID_MAX - 1);
        id0[0] = '/';
        id0[1] = 't';
        id0[2] = 'm';
        id0[3] = 'p';
        id0[4] = '/';
        id0[PROVIDER_ID_MAX - 2] = 'A';
        id0[PROVIDER_ID_MAX - 1] = '\0';
        inited = 1;
    }
    if (raw_idx == 0) {
        return id0;
    }
    return "short-row-id";
}

static int reset_provider_row_count(AppData *app) {
    (void)app;
    return 5;
}

static const char *reset_provider_row_identity(AppData *app, int raw_idx) {
    (void)app;
    switch (raw_idx) {
        case 0: return "row:zero";
        case 1: return "row:one";
        case 2: return "row:two";
        case 3: return "row:three";
        case 4: return "row:four";
        default: return NULL;
    }
}

static int find_filtered_index_by_glyph(AppData *app, const char *glyph) {
    for (int i = 0; i < app->filtered_emoji_count; i++) {
        if (strcmp(EMOJI_TABLE[app->filtered_emoji[i]].glyph, glyph) == 0) {
            return i;
        }
    }
    return -1;
}

static void test_selection_follows_identity_and_resets_when_missing(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    cofi_registry_reset();
    emoji_provider_register();

    const CofiTabProvider *provider = cofi_get_provider(s_emoji_provider_id);
    ASSERT_TRUE("emoji provider registered", provider != NULL);
    ASSERT_TRUE("emoji provider has row_identity", provider && provider->row_identity != NULL);
    ASSERT_TRUE("emoji provider has on_query_changed", provider && provider->on_query_changed != NULL);

    app.current_tab = provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;

    provider->on_query_changed(&app, "ro");
    int rocket_before = find_filtered_index_by_glyph(&app, "🚀");
    ASSERT_TRUE("rocket present in ro results", rocket_before >= 0);

    app.selection.provider_index = rocket_before;
    const char *selected_before = provider->row_identity(&app, app.selection.provider_index);
    ASSERT_TRUE("selected identity exists", selected_before && selected_before[0] != '\0');

    preserve_selection(&app);
    provider->on_query_changed(&app, "rocket");
    restore_selection(&app);

    const char *selected_after = provider->row_identity(&app, app.selection.provider_index);
    ASSERT_TRUE("selection follows same identity across filter", selected_after && strcmp(selected_after, "🚀") == 0);

    preserve_selection(&app);
    provider->on_query_changed(&app, "zzzzzzzz");
    restore_selection(&app);
    ASSERT_TRUE("selection resets to 0 when selected row disappears", app.selection.provider_index == 0);
}

static void test_long_identity_is_not_truncated(void) {
    AppData app;
    CofiTabProvider provider;
    memset(&app, 0, sizeof(app));
    memset(&provider, 0, sizeof(provider));

    cofi_registry_reset();

    provider.id = "longid";
    provider.display_name = "LongId";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.row_count = long_provider_row_count;
    provider.row_identity = long_provider_row_identity;
    provider.initial_selection_index = 0;
    s_long_provider_id = cofi_register_tab_provider(&provider);
    ASSERT_TRUE("longid provider registered", s_long_provider_id >= 0);

    const CofiTabProvider *registered = cofi_get_provider(s_long_provider_id);
    ASSERT_TRUE("registered longid provider available", registered != NULL);
    app.current_tab = (TabMode)registered->tab_mode;
    app.selection.provider_index = 0;

    preserve_selection(&app);
    ASSERT_TRUE("preserved provider id stores full identity",
                strcmp(app.selection.selected_provider_id,
                       long_provider_row_identity(&app, 0)) == 0);

    app.selection.provider_index = 1;
    restore_selection(&app);
    ASSERT_TRUE("restore follows long identity row", app.selection.provider_index == 0);
}

static void test_reset_selection_leaves_stale_provider_identity(void) {
    AppData app;
    CofiTabProvider provider;
    memset(&app, 0, sizeof(app));
    memset(&provider, 0, sizeof(provider));

    cofi_registry_reset();

    provider.id = "reset-provider";
    provider.display_name = "ResetProvider";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.row_count = reset_provider_row_count;
    provider.row_identity = reset_provider_row_identity;
    provider.initial_selection_index = 2;
    s_reset_provider_id = cofi_register_tab_provider(&provider);
    ASSERT_TRUE("reset provider registered", s_reset_provider_id >= 0);

    const CofiTabProvider *registered = cofi_get_provider(s_reset_provider_id);
    ASSERT_TRUE("registered reset provider available", registered != NULL);

    app.current_tab = (TabMode)registered->tab_mode;
    app.selection.provider_index = 3;
    app.selection.provider_scroll_offset = 7;
    snprintf(app.selection.selected_provider_id,
             sizeof(app.selection.selected_provider_id), "%s", "row:stale");

    reset_selection(&app);

    ASSERT_TRUE("reset uses provider initial selection",
                app.selection.provider_index == registered->initial_selection_index);
    ASSERT_TRUE("reset clears provider scroll offset",
                app.selection.provider_scroll_offset == 0);
    ASSERT_TRUE("reset leaves stale provider identity",
                strcmp(app.selection.selected_provider_id, "row:stale") == 0);
}

int main(void) {
    printf("Provider selection tests\n");
    printf("========================\n\n");

    test_selection_follows_identity_and_resets_when_missing();
    test_long_identity_is_not_truncated();
    test_reset_selection_leaves_stale_provider_identity();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
