#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"

#define TEST_DYNAMIC_TAB  ((TabMode)(TAB_COUNT + 1))
#define TEST_SESSIONS_TAB ((TabMode)(TAB_COUNT + 2))
#define TEST_PROFILES_TAB ((TabMode)(TAB_COUNT + 3))
#define TEST_HARPOON_TAB  ((TabMode)(TAB_COUNT + 4))
#define TEST_APPS_TAB     ((TabMode)(TAB_COUNT + 5))

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

int cofi_list_provider_tabs(int *tabs, int max_tabs) {
    int count = 0;
    if (max_tabs > count) {
        tabs[count++] = TEST_HARPOON_TAB;
    }
    if (max_tabs > count) {
        tabs[count++] = TEST_APPS_TAB;
    }
    if (max_tabs > count) {
        tabs[count++] = TEST_DYNAMIC_TAB;
    }
    if (max_tabs > count) {
        tabs[count++] = TEST_SESSIONS_TAB;
    }
    if (max_tabs > count) {
        tabs[count++] = TEST_PROFILES_TAB;
    }
    return count;
}

const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    static CofiTabProvider dynamic_provider;
    if (tab_mode == TEST_DYNAMIC_TAB) {
        memset(&dynamic_provider, 0, sizeof(dynamic_provider));
        dynamic_provider.id = "dynamic";
        dynamic_provider.display_name = "Dynamic";
        dynamic_provider.tab_mode = TEST_DYNAMIC_TAB;
        return &dynamic_provider;
    }
    if (tab_mode == TEST_SESSIONS_TAB) {
        memset(&dynamic_provider, 0, sizeof(dynamic_provider));
        dynamic_provider.id = "sessions";
        dynamic_provider.display_name = "Sessions";
        dynamic_provider.tab_mode = TEST_SESSIONS_TAB;
        return &dynamic_provider;
    }
    if (tab_mode == TEST_PROFILES_TAB) {
        memset(&dynamic_provider, 0, sizeof(dynamic_provider));
        dynamic_provider.id = "profiles";
        dynamic_provider.display_name = "Profiles";
        dynamic_provider.tab_mode = TEST_PROFILES_TAB;
        return &dynamic_provider;
    }
    if (tab_mode == TEST_APPS_TAB) {
        memset(&dynamic_provider, 0, sizeof(dynamic_provider));
        dynamic_provider.id = "apps";
        dynamic_provider.display_name = "Apps";
        dynamic_provider.tab_mode = TEST_APPS_TAB;
        return &dynamic_provider;
    }
    if (tab_mode == TEST_HARPOON_TAB) {
        memset(&dynamic_provider, 0, sizeof(dynamic_provider));
        dynamic_provider.id = "harpoon";
        dynamic_provider.display_name = "Harpoon";
        dynamic_provider.tab_mode = TEST_HARPOON_TAB;
        return &dynamic_provider;
    }
    return NULL;
}

gboolean tab_is_visible(AppData *app, TabMode tab) {
    if (!app || tab < TAB_WINDOWS || tab >= COFI_MAX_TAB_HANDLES) return FALSE;
    if (app->config.show_all_tabs) return TRUE;
    return app->tab_visibility[tab] != TAB_VIS_HIDDEN;
}

#include "../src/tab_header.c"

static void init_hidden_tabs(AppData *app) {
    memset(app, 0, sizeof(*app));
    for (int tab = TAB_WINDOWS; tab < COFI_MAX_TAB_HANDLES; tab++) {
        app->tab_visibility[tab] = TAB_VIS_HIDDEN;
    }
}

static void test_full_header_when_it_fits(void) {
    AppData app;
    init_hidden_tabs(&app);
    app.tab_visibility[TAB_WINDOWS] = TAB_VIS_PINNED;
    app.tab_visibility[TEST_APPS_TAB] = TAB_VIS_PINNED;

    GString *out = g_string_new("");
    tab_header_format(&app, TAB_WINDOWS, 120, out);

    ASSERT_TRUE("full header includes active windows",
                strstr(out->str, "[ WINDOWS ]") != NULL);
    ASSERT_TRUE("full header includes apps",
                strstr(out->str, "Apps") != NULL);
    ASSERT_TRUE("full header has no overflow markers",
                strchr(out->str, '<') == NULL && strchr(out->str, '>') == NULL);
    g_string_free(out, TRUE);
}

static void test_show_all_tabs_makes_hidden_tabs_visible(void) {
    AppData app;
    init_hidden_tabs(&app);
    app.tab_visibility[TAB_WINDOWS] = TAB_VIS_PINNED;
    app.tab_visibility[TEST_APPS_TAB] = TAB_VIS_PINNED;
    app.config.show_all_tabs = 1;

    GString *out = g_string_new("");
    tab_header_format(&app, TEST_SESSIONS_TAB, 200, out);

    ASSERT_TRUE("show all header includes hidden sessions",
                strstr(out->str, "[ SESSIONS ]") != NULL);
    ASSERT_TRUE("show all header includes hidden profiles",
                strstr(out->str, "Profiles") != NULL);
    g_string_free(out, TRUE);
}

static void test_dynamic_tab_renders_provider_name(void) {
    AppData app;
    init_hidden_tabs(&app);
    app.tab_visibility[TAB_WINDOWS] = TAB_VIS_PINNED;
    app.tab_visibility[TEST_DYNAMIC_TAB] = TAB_VIS_PINNED;

    GString *out = g_string_new("");
    tab_header_format(&app, TEST_DYNAMIC_TAB, 120, out);

    ASSERT_TRUE("dynamic tab renders provider display name",
                strstr(out->str, "[ DYNAMIC ]") != NULL);
    g_string_free(out, TRUE);
}

static void test_overflow_keeps_current_tab_visible(void) {
    AppData app;
    init_hidden_tabs(&app);
    app.config.show_all_tabs = 1;

    GString *out = g_string_new("");
    tab_header_format(&app, TEST_APPS_TAB, 48, out);

    ASSERT_TRUE("overflow includes active tab",
                strstr(out->str, "[ APPS ]") != NULL);
    ASSERT_TRUE("overflow shows left marker", strchr(out->str, '<') != NULL);
    ASSERT_TRUE("overflow shows right marker", strchr(out->str, '>') != NULL);
    const char *line_start = out->str[0] == '\n' ? out->str + 1 : out->str;
    const char *line_end = strchr(line_start, '\n');
    size_t line_len = line_end ? (size_t)(line_end - line_start) : strlen(line_start);
    ASSERT_TRUE("overflow line respects budget", line_len <= 48);
    g_string_free(out, TRUE);
}

int main(void) {
    printf("Tab header tests\n");
    printf("================\n\n");

    test_full_header_when_it_fits();
    test_show_all_tabs_makes_hidden_tabs_visible();
    test_dynamic_tab_renders_provider_name();
    test_overflow_keeps_current_tab_visible();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
