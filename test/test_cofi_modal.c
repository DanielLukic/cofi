#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_modal.h"
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
            printf("FAIL: %s\n", msg); \
        } \
    } while (0)

static int g_update_display_calls;
static int g_switch_to_tab_calls;
static int g_surface_tab_calls;

#define TEST_MODAL_TAB ((TabMode)(TAB_COUNT + 1))

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void hide_window(AppData *app) {
    if (app) app->window_visible = FALSE;
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

void update_scroll_position(AppData *app) {
    (void)app;
}

void clear_prefix_tab_claim(AppData *app) {
    if (app) app->active_prefix_claim = '\0';
}

void switch_to_tab(AppData *app, TabMode target_tab) {
    g_switch_to_tab_calls++;
    if (app) app->current_tab = target_tab;
}

void surface_tab(AppData *app, TabMode tab) {
    g_surface_tab_calls++;
    if (!app) return;
    app->tab_visibility[tab] = TAB_VIS_SURFACED;
    app->current_tab = tab;
}

static CofiTabProvider register_run_modal_provider(CofiModalPolicy policy) {
    cofi_registry_reset();
    CofiTabProvider p;
    cofi_init_provider_defaults(&p);
    p.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    p.id = "run";
    p.prefix_char = '!';
    p.modal_policy = policy;
    int provider_id = cofi_register_tab_provider(&p);
    const CofiTabProvider *registered = cofi_get_provider(provider_id);
    return registered ? *registered : p;
}

static void setup_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_update_display_calls = 0;
    g_switch_to_tab_calls = 0;
    g_surface_tab_calls = 0;
    app->entry = gtk_entry_new();
    app->mode_indicator = gtk_label_new(">");
    app->current_tab = TAB_WINDOWS;
    app->prefix_origin_tab = TAB_APPS;
    app->tab_visibility[TAB_WINDOWS] = TAB_VIS_PINNED;
    app->tab_visibility[TEST_MODAL_TAB] = TAB_VIS_HIDDEN;
    app->window_visible = TRUE;
}

static void test_enter_modal_sets_lifecycle_state(void) {
    AppData app;
    CofiTabProvider p = register_run_modal_provider(COFI_MODAL_CLEAR_THEN_RETURN);
    setup_app(&app);
    gtk_entry_set_text(GTK_ENTRY(app.entry), "dirty");

    cofi_enter_modal(&app, &p);

    ASSERT_TRUE("enter sets CMD_MODE_MODAL", app.command_mode.state == CMD_MODE_MODAL);
    ASSERT_TRUE("enter records prefix origin tab", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("enter surfaces provider tab",
                app.current_tab == TEST_MODAL_TAB &&
                app.tab_visibility[TEST_MODAL_TAB] == TAB_VIS_SURFACED);
    ASSERT_TRUE("enter sets mode indicator",
                strcmp(gtk_label_get_text(GTK_LABEL(app.mode_indicator)), "!") == 0);
    ASSERT_TRUE("enter clears entry",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "") == 0);
    ASSERT_TRUE("enter used surface_tab", g_surface_tab_calls == 1);
}

static void test_exit_modal_restores_origin_and_hides_tab(void) {
    AppData app;
    register_run_modal_provider(COFI_MODAL_CLEAR_THEN_RETURN);
    setup_app(&app);
    app.command_mode.state = CMD_MODE_MODAL;
    app.current_tab = TEST_MODAL_TAB;
    app.prefix_origin_tab = TAB_WINDOWS;
    app.active_prefix_claim = '!';
    app.tab_visibility[TEST_MODAL_TAB] = TAB_VIS_SURFACED;
    gtk_label_set_text(GTK_LABEL(app.mode_indicator), "!");

    cofi_exit_modal(&app);

    ASSERT_TRUE("exit restores normal mode", app.command_mode.state == CMD_MODE_NORMAL);
    ASSERT_TRUE("exit hides modal tab", app.tab_visibility[TEST_MODAL_TAB] == TAB_VIS_HIDDEN);
    ASSERT_TRUE("exit restores origin tab", app.current_tab == TAB_WINDOWS);
    ASSERT_TRUE("exit clears active prefix", app.active_prefix_claim == '\0');
    ASSERT_TRUE("exit clears indicator",
                strcmp(gtk_label_get_text(GTK_LABEL(app.mode_indicator)), ">") == 0);
    ASSERT_TRUE("exit switched to origin", g_switch_to_tab_calls == 1);
}

static void test_escape_nonempty_clears_only(void) {
    AppData app;
    register_run_modal_provider(COFI_MODAL_CLEAR_THEN_RETURN);
    setup_app(&app);
    app.command_mode.state = CMD_MODE_MODAL;
    app.current_tab = TEST_MODAL_TAB;
    app.prefix_origin_tab = TAB_WINDOWS;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "echo hi");

    GdkEventKey event = {.keyval = GDK_KEY_Escape};
    gboolean handled = cofi_handle_modal_key(&app, &event);

    ASSERT_TRUE("esc nonempty handled", handled == TRUE);
    ASSERT_TRUE("esc nonempty keeps modal", app.command_mode.state == CMD_MODE_MODAL);
    ASSERT_TRUE("esc nonempty stays on modal tab", app.current_tab == TEST_MODAL_TAB);
    ASSERT_TRUE("esc nonempty clears entry",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "") == 0);
    ASSERT_TRUE("esc nonempty refreshes display", g_update_display_calls == 1);
}

static void test_escape_empty_exits_modal(void) {
    AppData app;
    register_run_modal_provider(COFI_MODAL_CLEAR_THEN_RETURN);
    setup_app(&app);
    app.command_mode.state = CMD_MODE_MODAL;
    app.current_tab = TEST_MODAL_TAB;
    app.prefix_origin_tab = TAB_WINDOWS;
    app.tab_visibility[TEST_MODAL_TAB] = TAB_VIS_SURFACED;

    GdkEventKey event = {.keyval = GDK_KEY_Escape};
    gboolean handled = cofi_handle_modal_key(&app, &event);

    ASSERT_TRUE("esc empty handled", handled == TRUE);
    ASSERT_TRUE("esc empty exits modal", app.command_mode.state == CMD_MODE_NORMAL);
    ASSERT_TRUE("esc empty restores origin", app.current_tab == TAB_WINDOWS);
    ASSERT_TRUE("esc empty hides tab", app.tab_visibility[TEST_MODAL_TAB] == TAB_VIS_HIDDEN);
}

static void test_tab_exits_modal_and_allows_tab_cycle(void) {
    AppData app;
    register_run_modal_provider(COFI_MODAL_CLEAR_THEN_RETURN);
    setup_app(&app);
    app.command_mode.state = CMD_MODE_MODAL;
    app.current_tab = TEST_MODAL_TAB;
    app.prefix_origin_tab = TAB_WINDOWS;
    app.tab_visibility[TEST_MODAL_TAB] = TAB_VIS_SURFACED;

    GdkEventKey event = {.keyval = GDK_KEY_Tab};
    gboolean handled = cofi_handle_modal_key(&app, &event);

    ASSERT_TRUE("tab is left for normal tab cycling", handled == FALSE);
    ASSERT_TRUE("tab exits modal", app.command_mode.state == CMD_MODE_NORMAL);
    ASSERT_TRUE("tab restores origin", app.current_tab == TAB_WINDOWS);
    ASSERT_TRUE("tab hides modal tab", app.tab_visibility[TEST_MODAL_TAB] == TAB_VIS_HIDDEN);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("SKIP: GTK unavailable\n");
        return 0;
    }

    printf("cofi_modal behavioral tests\n");
    printf("===========================\n\n");

    test_enter_modal_sets_lifecycle_state();
    test_exit_modal_restores_origin_and_hides_tab();
    test_escape_nonempty_clears_only();
    test_escape_empty_exits_modal();
    test_tab_exits_modal_and_allows_tab_cycle();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
