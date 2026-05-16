#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/browser_profiles.h"
#include "../src/cofi_tab_provider.h"

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

static int g_reset_selection_calls;
static int g_launch_calls;
static char g_last_launch_arg0[256];
static char g_last_launch_arg1[256];

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void reset_selection(AppData *app) {
    (void)app;
    g_reset_selection_calls++;
}

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    (void)p;
    return 0;
}

gboolean detach_launch_argv_array(const char *const *argv) {
    (void)argv;
    return TRUE;
}

static gchar *fake_program_resolver(const char *program) {
    if (program && strcmp(program, "google-chrome") == 0) {
        return g_strdup("/usr/bin/google-chrome");
    }
    return NULL;
}

static gboolean fake_launch_impl(const char *const *argv) {
    g_launch_calls++;
    g_strlcpy(g_last_launch_arg0, argv && argv[0] ? argv[0] : "",
              sizeof(g_last_launch_arg0));
    g_strlcpy(g_last_launch_arg1, argv && argv[1] ? argv[1] : "",
              sizeof(g_last_launch_arg1));
    return TRUE;
}

#include "../src/browser_profiles.c"
#include "../src/profiles_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    init_browser_profiles_mode(&s_profiles_mode);
    g_reset_selection_calls = 0;
    g_launch_calls = 0;
    g_last_launch_arg0[0] = '\0';
    g_last_launch_arg1[0] = '\0';
    browser_profiles_set_program_resolver_test_hook(fake_program_resolver);
    browser_profiles_set_launch_impl_test_hook(fake_launch_impl);
}

static void seed_profiles(void) {
    s_profiles_mode.profile_count = 2;
    s_profiles_mode.filtered_count = 2;
    s_profiles_mode.filtered_indices[0] = 0;
    s_profiles_mode.filtered_indices[1] = 1;

    BrowserProfileEntry *first = &s_profiles_mode.profiles[0];
    memset(first, 0, sizeof(*first));
    first->backend = BROWSER_PROFILE_CHROME;
    g_strlcpy(first->browser_id, "chrome", sizeof(first->browser_id));
    g_strlcpy(first->browser_name, "Chrome", sizeof(first->browser_name));
    g_strlcpy(first->executable, "google-chrome", sizeof(first->executable));
    g_strlcpy(first->profile_dir, "Profile 14", sizeof(first->profile_dir));
    g_strlcpy(first->name, "GS", sizeof(first->name));
    g_strlcpy(first->email, "profile.primary@example.test", sizeof(first->email));
    first->active_time = 200.0;

    BrowserProfileEntry *second = &s_profiles_mode.profiles[1];
    memset(second, 0, sizeof(*second));
    second->backend = BROWSER_PROFILE_CHROME;
    g_strlcpy(second->browser_id, "chrome", sizeof(second->browser_id));
    g_strlcpy(second->browser_name, "Chrome", sizeof(second->browser_name));
    g_strlcpy(second->executable, "google-chrome", sizeof(second->executable));
    g_strlcpy(second->profile_dir, "Default", sizeof(second->profile_dir));
    g_strlcpy(second->name, "Drew", sizeof(second->name));
    g_strlcpy(second->email, "profile.secondary@example.test", sizeof(second->email));
    second->active_time = 100.0;
}

static void test_empty_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("empty provider exposes one status row", profiles_row_count(&app) == 1);

    memset(&row, 0, sizeof(row));
    profiles_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row text", strcmp(row.cells[0].text, "No matching browser profiles found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
}

static void test_format_profile_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_profiles();

    ASSERT_TRUE("profile row count", profiles_row_count(&app) == 2);

    memset(&row, 0, sizeof(row));
    profiles_format_row(&app, 0, &row);
    ASSERT_TRUE("profile row has four cells", row.cell_count == 4);
    ASSERT_TRUE("profile row marker", strcmp(row.cells[0].text, "[c]") == 0);
    ASSERT_TRUE("profile row name", strcmp(row.cells[1].text, "GS") == 0);
    ASSERT_TRUE("profile row email", strcmp(row.cells[2].text, "profile.primary@example.test") == 0);
    ASSERT_TRUE("profile row dir", strcmp(row.cells[3].text, "Profile 14") == 0);
    ASSERT_TRUE("profile row actionable", row.row_flags == COFI_ROW_ACTIONABLE);
}

static void test_query_filters_and_resets_selection(void) {
    AppData app;
    reset_state(&app);
    seed_profiles();

    profiles_on_query_changed(&app, "gs");

    ASSERT_TRUE("query resets selection", g_reset_selection_calls == 1);
    ASSERT_TRUE("query filters to one profile", s_profiles_mode.filtered_count == 1);
    ASSERT_TRUE("query keeps GS profile", s_profiles_mode.filtered_indices[0] == 0);
}

static void test_row_identity_and_match_string(void) {
    AppData app;
    reset_state(&app);
    seed_profiles();

    ASSERT_TRUE("row identity uses backend and dir",
                strcmp(profiles_row_identity(&app, 0), "chrome:Profile 14") == 0);
    ASSERT_TRUE("match string includes short marker",
                strstr(profiles_match_string(&app, 0), "[c]") != NULL);
    ASSERT_TRUE("match string includes email",
                strstr(profiles_match_string(&app, 0), "profile.primary") != NULL);
}

static void test_enter_launches_profile(void) {
    AppData app;
    reset_state(&app);
    seed_profiles();

    CofiActionStatus status = profiles_on_enter_pressed(&app, 0, 0, "", 0);

    ASSERT_TRUE("enter returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("launch called once", g_launch_calls == 1);
    ASSERT_TRUE("launch uses resolved shim",
                strcmp(g_last_launch_arg0, "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("launch uses profile dir",
                strcmp(g_last_launch_arg1, "--profile-directory=Profile 14") == 0);
}

int main(void) {
    printf("Profiles provider tests\n");
    printf("=======================\n\n");

    test_empty_row();
    test_format_profile_row();
    test_query_filters_and_resets_selection();
    test_row_identity_and_match_string();
    test_enter_launches_profile();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
