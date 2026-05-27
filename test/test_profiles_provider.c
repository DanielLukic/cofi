#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "profiles/browser_profiles.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_registry.h"
#include "core/slot_store/slot_store.h"

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
static int g_hide_window_calls;
static int g_exit_command_mode_calls;
static int g_registered_provider_id = -1;
static char g_last_launch_arg0[256];
static char g_last_launch_arg1[256];
static CofiTabProvider g_registered_provider;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void reset_selection(AppData *app) {
    (void)app;
    g_reset_selection_calls++;
}

void exit_command_mode(AppData *app) {
    (void)app;
    g_exit_command_mode_calls++;
}

void hide_window(AppData *app) {
    (void)app;
    g_hide_window_calls++;
}

void surface_tab(AppData *app, TabMode tab) {
    if (app) app->current_tab = tab;
}

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
    if (p) g_registered_provider = *p;
    if (g_registered_provider.tab_mode == COFI_PROVIDER_DYNAMIC_TAB) {
        g_registered_provider.tab_mode = TAB_COUNT + 1;
    }
    g_registered_provider_id = 0;
    return g_registered_provider_id;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    return provider_id == g_registered_provider_id ? &g_registered_provider : NULL;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
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

#include "profiles/browser_profiles.c"
#include "profiles/profiles_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    init_browser_profiles_mode(&s_profiles_mode);
    g_reset_selection_calls = 0;
    g_launch_calls = 0;
    g_hide_window_calls = 0;
    g_exit_command_mode_calls = 0;
    g_registered_provider_id = -1;
    g_last_launch_arg0[0] = '\0';
    g_last_launch_arg1[0] = '\0';
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
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
    ASSERT_TRUE("profile row marker", strcmp(row.cells[0].text, "[gc]") == 0);
    ASSERT_TRUE("profile row name", strcmp(row.cells[1].text, "GS") == 0);
    ASSERT_TRUE("profile row email", strcmp(row.cells[2].text, "profile.primary@example.test") == 0);
    ASSERT_TRUE("profile row dir", strcmp(row.cells[3].text, "Profile 14") == 0);
    ASSERT_TRUE("profile row actionable and slottable",
                row.row_flags == (COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE));
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
                strstr(profiles_match_string(&app, 0), "[gc]") != NULL);
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

static void test_slot_payload_and_recall(void) {
    AppData app;
    reset_state(&app);
    seed_profiles();

    const char *payload = profiles_slot_payload_for(&app, 0);
    ASSERT_TRUE("slot payload uses backend and profile dir",
                strcmp(payload, "profile:chrome:Profile 14") == 0);

    CofiActionStatus status = profiles_slot_recall(&app, payload);

    ASSERT_TRUE("slot recall returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("slot recall launches once", g_launch_calls == 1);
    ASSERT_TRUE("slot recall uses resolved shim",
                strcmp(g_last_launch_arg0, "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("slot recall uses profile dir",
                strcmp(g_last_launch_arg1, "--profile-directory=Profile 14") == 0);
}

static void test_command_at_slot_recalls_profile(void) {
    AppData app;
    reset_state(&app);
    slot_store_init(&app.harpoon.store);
    slot_assign(&app.harpoon.store, 'a', "profiles", "profile:chrome:Default");

    CofiActionStatus status = profiles_on_command_args(&app, "@a");

    ASSERT_TRUE("command @slot returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("command @slot launches once", g_launch_calls == 1);
    ASSERT_TRUE("command @slot uses profile dir",
                strcmp(g_last_launch_arg1, "--profile-directory=Default") == 0);
    slot_store_free(&app.harpoon.store);
}

static void test_provider_registers_command_metadata(void) {
    AppData app;
    reset_state(&app);

    profiles_provider_register();

    ASSERT_TRUE("profiles primary command registered",
                strcmp(s_profiles_command.primary, "profiles") == 0);
    ASSERT_TRUE("profiles chrome alias registered",
                strcmp(s_profiles_command.aliases[0], "chrome") == 0);
    ASSERT_TRUE("profiles help format registered",
                strcmp(s_profiles_command.help_format,
                       "profiles, chrome [@SLOT|PROFILE]") == 0);
    ASSERT_TRUE("profiles command handler registered",
                s_profiles_command.handler != NULL);
    ASSERT_TRUE("profiles command keeps cofi open for auto hotkeys",
                s_profiles_command.keeps_open_on_hotkey_auto == 1);
}

static void test_command_handler_surfaces_and_recalls_slots(void) {
    AppData app;
    reset_state(&app);
    profiles_provider_register();

    app.current_tab = TAB_WINDOWS;
    gboolean result = s_profiles_command.handler(&app, NULL, "");
    ASSERT_TRUE("command without args returns false", result == FALSE);
    ASSERT_TRUE("command without args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("command without args surfaces profiles tab",
                app.current_tab == (TabMode)g_registered_provider.tab_mode);
    ASSERT_TRUE("command without args does not hide", g_hide_window_calls == 0);

    slot_store_init(&app.harpoon.store);
    slot_assign(&app.harpoon.store, 'a', "profiles", "profile:chrome:Default");
    g_exit_command_mode_calls = 0;
    g_hide_window_calls = 0;
    g_launch_calls = 0;

    result = s_profiles_command.handler(&app, NULL, "@a");
    ASSERT_TRUE("command with slot returns false", result == FALSE);
    ASSERT_TRUE("command with slot exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("command with slot launches profile", g_launch_calls == 1);
    ASSERT_TRUE("command with slot hides after launch", g_hide_window_calls == 1);
    ASSERT_TRUE("command with slot uses profile directory",
                strcmp(g_last_launch_arg1, "--profile-directory=Default") == 0);
    slot_store_free(&app.harpoon.store);
}

int main(void) {
    printf("Profiles provider tests\n");
    printf("=======================\n\n");

    test_empty_row();
    test_format_profile_row();
    test_query_filters_and_resets_selection();
    test_row_identity_and_match_string();
    test_enter_launches_profile();
    test_slot_payload_and_recall();
    test_command_at_slot_recalls_profile();
    test_provider_registers_command_metadata();
    test_command_handler_surfaces_and_recalls_slots();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
