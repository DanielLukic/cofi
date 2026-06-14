#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "bookmarks/bookmarks.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_registry.h"
#include "core/slot_store/slot_store.h"
#include "profiles/browser_profiles.h"
#include "profiles/chrome_launch.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { tests_passed++; printf("PASS: %s\n", msg); } \
        else { printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
    } while (0)

static int g_reset_selection_calls;
static int g_launch_calls;
static int g_exit_command_mode_calls;
static int g_hide_window_calls;
static int g_registered_provider_id = -1;
static char g_last_launch_arg0[512];
static char g_last_launch_arg1[512];
static char g_last_launch_arg2[512];
static CofiTabProvider g_registered_provider;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}
void reset_selection(AppData *app) { (void)app; g_reset_selection_calls++; }
void exit_command_mode(AppData *app) { (void)app; g_exit_command_mode_calls++; }
void hide_window(AppData *app) { (void)app; g_hide_window_calls++; }
void surface_tab(AppData *app, TabMode tab) { if (app) app->current_tab = tab; }

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

int cofi_register_command(const CommandSpec *spec) { return spec ? 0 : -1; }

gboolean detach_launch_argv_array(const char *const *argv) {
    (void)argv;
    return TRUE;
}

static gchar *fake_resolver(const char *program) {
    if (program && strcmp(program, "google-chrome") == 0) {
        return g_strdup("/usr/bin/google-chrome");
    }
    return NULL;
}

static gboolean fake_launch_impl(const char *const *argv) {
    g_launch_calls++;
    g_strlcpy(g_last_launch_arg0, argv && argv[0] ? argv[0] : "", sizeof(g_last_launch_arg0));
    g_strlcpy(g_last_launch_arg1, argv && argv[1] ? argv[1] : "", sizeof(g_last_launch_arg1));
    g_strlcpy(g_last_launch_arg2, argv && argv[2] ? argv[2] : "", sizeof(g_last_launch_arg2));
    return TRUE;
}

#include "bookmarks/bookmarks.c"
#include "bookmarks/bookmarks_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    bookmarks_mode_free(&s_bookmarks_mode);
    bookmarks_mode_init(&s_bookmarks_mode);
    g_reset_selection_calls = 0;
    g_launch_calls = 0;
    g_exit_command_mode_calls = 0;
    g_hide_window_calls = 0;
    g_registered_provider_id = -1;
    g_last_launch_arg0[0] = g_last_launch_arg1[0] = g_last_launch_arg2[0] = '\0';
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
    chrome_launch_set_program_resolver_test_hook(fake_resolver);
    bookmarks_provider_set_launch_impl_for_test(fake_launch_impl);
}

static void seed_one(const char *profile_dir, const char *label,
                     const char *folder, const char *name, const char *url) {
    BookmarkEntry e;
    memset(&e, 0, sizeof(e));
    g_strlcpy(e.profile_dir, profile_dir, sizeof(e.profile_dir));
    g_strlcpy(e.profile_label, label, sizeof(e.profile_label));
    g_strlcpy(e.folder_breadcrumb, folder, sizeof(e.folder_breadcrumb));
    g_strlcpy(e.name, name, sizeof(e.name));
    g_strlcpy(e.url, url, sizeof(e.url));
    g_array_append_val(s_bookmarks_mode.entries, e);
}

static void seed_two_bookmarks(void) {
    seed_one("Default", "DefaultUser", "", "First Page", "https://first.example/p");
    seed_one("Profile 7", "Alt", "Docs > Deep",
             "Second", "https://second.example/path?x=1");
    int zero = 0, one = 1;
    g_array_append_val(s_bookmarks_mode.filtered_indices, zero);
    g_array_append_val(s_bookmarks_mode.filtered_indices, one);
}

static void test_empty_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("empty provider exposes one status row",
                bookmarks_row_count(&app) == 1);

    memset(&row, 0, sizeof(row));
    bookmarks_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row uses no-match text",
                strcmp(row.cells[0].text, "No matching bookmarks found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
}

static void test_empty_row_with_error(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    g_strlcpy(s_bookmarks_mode.last_error, "Failed to load Chrome bookmarks",
              sizeof(s_bookmarks_mode.last_error));

    memset(&row, 0, sizeof(row));
    bookmarks_format_row(&app, 0, &row);
    ASSERT_TRUE("error row shows last_error",
                strcmp(row.cells[0].text, "Failed to load Chrome bookmarks") == 0);
    ASSERT_TRUE("error row flagged ROW_ERROR",
                row.row_flags == COFI_ROW_ERROR);
}

static void test_format_bookmark_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_two_bookmarks();

    ASSERT_TRUE("row count uses filtered_count",
                bookmarks_row_count(&app) == 2);

    memset(&row, 0, sizeof(row));
    bookmarks_format_row(&app, 0, &row);
    ASSERT_TRUE("row has five cells", row.cell_count == 5);
    ASSERT_TRUE("cell 0 is [bm]", strcmp(row.cells[0].text, "[bm]") == 0);
    ASSERT_TRUE("cell 1 is profile label",
                strcmp(row.cells[1].text, "DefaultUser") == 0);
    ASSERT_TRUE("cell 2 is bookmark name",
                strcmp(row.cells[2].text, "First Page") == 0);
    ASSERT_TRUE("cell 3 is folder (top-level empty)",
                strcmp(row.cells[3].text, "") == 0);
    ASSERT_TRUE("cell 4 is url",
                strcmp(row.cells[4].text, "https://first.example/p") == 0);
    ASSERT_TRUE("row actionable and slottable",
                row.row_flags == (COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE));
}

static void test_folder_truncated_in_display(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_two_bookmarks();
    memset(&row, 0, sizeof(row));
    bookmarks_format_row(&app, 1, &row);
    ASSERT_TRUE("long breadcrumb left-clipped in display",
                g_str_has_prefix(row.cells[3].text, "…") ||
                strlen(row.cells[3].text) <= 14 /* utf-8 ellipsis = 3 bytes */);
}

static void test_row_identity_and_match_string(void) {
    AppData app;
    reset_state(&app);
    seed_two_bookmarks();
    ASSERT_TRUE("identity uses bookmark:<dir>:<url>",
                strcmp(bookmarks_row_identity(&app, 0),
                       "bookmark:Default:https://first.example/p") == 0);
    ASSERT_TRUE("match string starts with [bm]",
                g_str_has_prefix(bookmarks_match_string(&app, 0), "[bm]"));
}

static void test_enter_launches_url(void) {
    AppData app;
    reset_state(&app);
    seed_two_bookmarks();
    CofiActionStatus status = bookmarks_on_enter_pressed(&app, 0, 0, "", 0);
    ASSERT_TRUE("enter returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("launch called once", g_launch_calls == 1);
    ASSERT_TRUE("launch path is resolved chrome",
                strcmp(g_last_launch_arg0, "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("launch profile flag",
                strcmp(g_last_launch_arg1, "--profile-directory=Default") == 0);
    ASSERT_TRUE("launch url",
                strcmp(g_last_launch_arg2, "https://first.example/p") == 0);
}

static void test_slot_payload_roundtrip(void) {
    AppData app;
    reset_state(&app);
    seed_two_bookmarks();
    const char *payload = bookmarks_slot_payload_for(&app, 1);
    ASSERT_TRUE("payload uses bookmark:chrome:<dir>:<url> format",
                strcmp(payload,
                       "bookmark:chrome:Profile 7:https://second.example/path?x=1") == 0);

    CofiActionStatus status = bookmarks_slot_recall(&app, payload);
    ASSERT_TRUE("recall returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("recall launched once", g_launch_calls == 1);
    ASSERT_TRUE("recall uses profile from payload",
                strcmp(g_last_launch_arg1, "--profile-directory=Profile 7") == 0);
    ASSERT_TRUE("recall preserves url with embedded ?x=1",
                strcmp(g_last_launch_arg2, "https://second.example/path?x=1") == 0);
}

static void test_slot_payload_oversize_rejected(void) {
    AppData app;
    reset_state(&app);
    /* Build an oversize URL that pushes the formatted payload past
     * SLOT_STORE_PAYLOAD_LEN. */
    char big_url[BOOKMARK_URL_LEN];
    memset(big_url, 'a', sizeof(big_url) - 1);
    big_url[0] = 'h'; big_url[1] = 't'; big_url[2] = 't'; big_url[3] = 'p';
    big_url[4] = ':'; big_url[5] = '/'; big_url[6] = '/';
    big_url[sizeof(big_url) - 1] = '\0';
    seed_one("Default", "U", "", "Big", big_url);
    int idx = 0;
    g_array_append_val(s_bookmarks_mode.filtered_indices, idx);

    const char *payload = bookmarks_slot_payload_for(&app, 0);
    ASSERT_TRUE("oversize payload rejected", payload == NULL);
}

static void test_slot_payload_preserves_colons_in_url(void) {
    AppData app;
    reset_state(&app);
    seed_one("Default", "U", "", "Colon",
             "https://example.test/a:b:c:d?q=1:2");
    int idx = 0;
    g_array_append_val(s_bookmarks_mode.filtered_indices, idx);
    const char *payload = bookmarks_slot_payload_for(&app, 0);
    CofiActionStatus status = bookmarks_slot_recall(&app, payload);
    ASSERT_TRUE("colon-url recall returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("colon-url URL preserved end-to-end",
                strcmp(g_last_launch_arg2,
                       "https://example.test/a:b:c:d?q=1:2") == 0);
}

static void test_command_args_query_no_match(void) {
    AppData app;
    reset_state(&app);
    /* Sandbox HOME so bookmarks_load finds no Chrome profiles and the query
     * cannot match anything regardless of host. */
    gchar *tmp = g_dir_make_tmp("cofi_bm_test_XXXXXX", NULL);
    g_setenv("HOME", tmp, TRUE);
    CofiActionStatus status = bookmarks_on_command_args(&app, "nothing");
    ASSERT_TRUE("no-match query returns action error", status == COFI_ACTION_ERROR);
    g_rmdir(tmp);
    g_free(tmp);
}

static void test_command_at_slot_recalls(void) {
    AppData app;
    reset_state(&app);
    slot_store_init(&app.harpoon.store);
    slot_assign(&app.harpoon.store, 'a', "bookmarks",
                "bookmark:chrome:Default:https://slot.example/");
    CofiActionStatus status = bookmarks_on_command_args(&app, "@a");
    ASSERT_TRUE("command @slot returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("command @slot launched once", g_launch_calls == 1);
    ASSERT_TRUE("command @slot url",
                strcmp(g_last_launch_arg2, "https://slot.example/") == 0);
    slot_store_free(&app.harpoon.store);
}

static void test_command_at_slot_invalid(void) {
    AppData app;
    reset_state(&app);
    slot_store_init(&app.harpoon.store);
    CofiActionStatus status = bookmarks_on_command_args(&app, "@a");
    ASSERT_TRUE("missing slot returns action error", status == COFI_ACTION_ERROR);
    slot_store_free(&app.harpoon.store);
}

static void test_provider_registers_command_metadata(void) {
    AppData app;
    reset_state(&app);
    bookmarks_provider_register();
    ASSERT_TRUE("primary command bookmarks",
                strcmp(s_bookmarks_command.primary, "bookmarks") == 0);
    ASSERT_TRUE("alias bm",
                strcmp(s_bookmarks_command.aliases[0], "bm") == 0);
    ASSERT_TRUE("description",
                strcmp(s_bookmarks_command.description,
                       "Switch to bookmarks tab") == 0);
    ASSERT_TRUE("help_format",
                strcmp(s_bookmarks_command.help_format,
                       "bookmarks, bm [@SLOT|QUERY]") == 0);
    ASSERT_TRUE("keeps_open_on_hotkey_auto set",
                s_bookmarks_command.keeps_open_on_hotkey_auto == 1);
}

static void test_provider_registration_flags(void) {
    AppData app;
    reset_state(&app);
    bookmarks_provider_register();
    ASSERT_TRUE("display name BOOKMARKS",
                strcmp(g_registered_provider.display_name, "BOOKMARKS") == 0);
    ASSERT_TRUE("hidden by default",
                g_registered_provider.hidden_by_default == 1);
    ASSERT_TRUE("hide-on-esc modal policy",
                g_registered_provider.modal_policy == COFI_MODAL_HIDE_ON_ESC);
    ASSERT_TRUE("initial selection 0",
                g_registered_provider.initial_selection_index == 0);
    ASSERT_TRUE("slot store enabled",
                g_registered_provider.slot_store_enabled == 1);
    ASSERT_TRUE("shortcut hint verbatim",
                strcmp(g_registered_provider.shortcut_hint,
                       "Shortcuts: Enter=Open  Ctrl+key=Assign slot  Alt+key=Recall slot") == 0);
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    bookmarks_provider_register();
    app.current_tab = TAB_WINDOWS;
    gboolean result = s_bookmarks_command.handler(&app, NULL, "");
    ASSERT_TRUE("bare command returns false", result == FALSE);
    ASSERT_TRUE("bare command exits command mode",
                g_exit_command_mode_calls == 1);
    ASSERT_TRUE("bare command surfaces bookmarks tab",
                app.current_tab == (TabMode)g_registered_provider.tab_mode);
}

int main(void) {
    printf("Bookmarks provider tests\n");
    printf("========================\n\n");

    test_empty_row();
    test_empty_row_with_error();
    test_format_bookmark_row();
    test_folder_truncated_in_display();
    test_row_identity_and_match_string();
    test_enter_launches_url();
    test_slot_payload_roundtrip();
    test_slot_payload_oversize_rejected();
    test_slot_payload_preserves_colons_in_url();
    test_command_args_query_no_match();
    test_command_at_slot_recalls();
    test_command_at_slot_invalid();
    test_provider_registers_command_metadata();
    test_provider_registration_flags();
    test_command_handler_surfaces_tab();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
