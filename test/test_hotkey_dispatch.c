#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/hotkey_dispatch.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#define TEST_WORKSPACES_TAB ((TabMode)(TAB_COUNT + 1))
#define TEST_HARPOON_TAB    ((TabMode)(TAB_COUNT + 2))

static int show_window_calls;
static int surface_tab_calls;
static int grab_focus_calls;
static int enter_modal_calls;
static int workspaces_enabled;
static int harpoon_enabled;
static int run_enabled;
static CofiTabProvider workspaces_provider;
static CofiTabProvider harpoon_provider;
static CofiTabProvider run_provider;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

int get_active_window_id(Display *display) {
    (void)display;
    return 0;
}

void show_window(AppData *app) {
    (void)app;
    show_window_calls++;
}

void enter_command_mode(AppData *app) {
    if (app) app->command_mode.state = CMD_MODE_COMMAND;
}

void exit_command_mode(AppData *app) {
    if (app) app->command_mode.state = CMD_MODE_NORMAL;
}

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    (void)provider;
    enter_modal_calls++;
    if (app) app->command_mode.state = CMD_MODE_MODAL;
}

void cofi_exit_modal(AppData *app) {
    if (app) app->command_mode.state = CMD_MODE_NORMAL;
}

void ensure_cofi_on_current_workspace(AppData *app) {
    (void)app;
}

void move_selection_up(AppData *app) {
    if (app) app->selection.window_index++;
}

void filter_windows(AppData *app, const char *filter) {
    (void)app;
    (void)filter;
}

void reset_selection(AppData *app) {
    (void)app;
}

void update_display(AppData *app) {
    (void)app;
}

void surface_tab(AppData *app, TabMode tab) {
    surface_tab_calls++;
    if (app) app->current_tab = tab;
}

void gtk_entry_set_text(GtkEntry *entry, const gchar *text) {
    (void)entry;
    (void)text;
}

void gtk_widget_grab_focus(GtkWidget *widget) {
    (void)widget;
    grab_focus_calls++;
}

const CofiTabProvider *cofi_get_provider_for_hotkey_mode(int mode) {
    if (mode == SHOW_MODE_WORKSPACES) {
        return workspaces_enabled ? &workspaces_provider : NULL;
    }
    if (mode == SHOW_MODE_HARPOON) {
        return harpoon_enabled ? &harpoon_provider : NULL;
    }
    if (mode == SHOW_MODE_RUN) {
        return run_enabled ? &run_provider : NULL;
    }
    return NULL;
}

#include "../src/hotkey_dispatch.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = TAB_WINDOWS;
    app->entry = (GtkWidget *)0x1;
    show_window_calls = 0;
    surface_tab_calls = 0;
    grab_focus_calls = 0;
    enter_modal_calls = 0;
    workspaces_enabled = 1;
    harpoon_enabled = 1;
    run_enabled = 1;
    memset(&workspaces_provider, 0, sizeof(workspaces_provider));
    workspaces_provider.id = "workspaces";
    workspaces_provider.tab_mode = TEST_WORKSPACES_TAB;
    workspaces_provider.hotkey_mode_claim = COFI_PROVIDER_HOTKEY_MODE(SHOW_MODE_WORKSPACES);
    memset(&harpoon_provider, 0, sizeof(harpoon_provider));
    harpoon_provider.id = "harpoon";
    harpoon_provider.tab_mode = TEST_HARPOON_TAB;
    harpoon_provider.hotkey_mode_claim = COFI_PROVIDER_HOTKEY_MODE(SHOW_MODE_HARPOON);
    memset(&run_provider, 0, sizeof(run_provider));
    run_provider.id = "run";
    run_provider.tab_mode = (TabMode)(TAB_COUNT + 3);
    run_provider.prefix_char = '!';
    run_provider.hotkey_mode_claim = COFI_PROVIDER_HOTKEY_MODE(SHOW_MODE_RUN);
}

static void test_hidden_workspaces_hotkey_surfaces_enabled_provider(void) {
    AppData app;
    reset_state(&app);
    app.window_visible = FALSE;

    dispatch_hotkey_mode(&app, SHOW_MODE_WORKSPACES);

    ASSERT_TRUE("enabled hidden workspaces hotkey shows window",
                show_window_calls == 1);
    ASSERT_TRUE("enabled hidden workspaces hotkey surfaces tab",
                surface_tab_calls == 1);
    ASSERT_TRUE("enabled hidden workspaces hotkey switches tab",
                app.current_tab == TEST_WORKSPACES_TAB);
}

static void test_hidden_harpoon_hotkey_surfaces_enabled_provider(void) {
    AppData app;
    reset_state(&app);
    app.window_visible = FALSE;

    dispatch_hotkey_mode(&app, SHOW_MODE_HARPOON);

    ASSERT_TRUE("enabled hidden harpoon hotkey shows window",
                show_window_calls == 1);
    ASSERT_TRUE("enabled hidden harpoon hotkey surfaces tab",
                surface_tab_calls == 1);
    ASSERT_TRUE("enabled hidden harpoon hotkey switches tab",
                app.current_tab == TEST_HARPOON_TAB);
}

static void test_visible_harpoon_hotkey_surfaces_enabled_provider(void) {
    AppData app;
    reset_state(&app);
    app.window_visible = TRUE;

    dispatch_hotkey_mode(&app, SHOW_MODE_HARPOON);

    ASSERT_TRUE("enabled visible harpoon hotkey surfaces tab",
                surface_tab_calls == 1);
    ASSERT_TRUE("enabled visible harpoon hotkey switches tab",
                app.current_tab == TEST_HARPOON_TAB);
    ASSERT_TRUE("enabled visible harpoon hotkey grabs focus",
                grab_focus_calls == 1);
}

static void test_hidden_harpoon_hotkey_ignores_disabled_provider(void) {
    AppData app;
    reset_state(&app);
    app.window_visible = FALSE;
    harpoon_enabled = 0;

    dispatch_hotkey_mode(&app, SHOW_MODE_HARPOON);

    ASSERT_TRUE("disabled hidden harpoon hotkey does not show window",
                show_window_calls == 0);
    ASSERT_TRUE("disabled hidden harpoon hotkey does not surface tab",
                surface_tab_calls == 0);
    ASSERT_TRUE("disabled hidden harpoon hotkey keeps current tab",
                app.current_tab == TAB_WINDOWS);
}

static void test_hidden_workspaces_hotkey_ignores_disabled_provider(void) {
    AppData app;
    reset_state(&app);
    app.window_visible = FALSE;
    workspaces_enabled = 0;

    dispatch_hotkey_mode(&app, SHOW_MODE_WORKSPACES);

    ASSERT_TRUE("disabled hidden workspaces hotkey does not show window",
                show_window_calls == 0);
    ASSERT_TRUE("disabled hidden workspaces hotkey does not surface tab",
                surface_tab_calls == 0);
    ASSERT_TRUE("disabled hidden workspaces hotkey keeps current tab",
                app.current_tab == TAB_WINDOWS);
}

static void test_visible_workspaces_hotkey_ignores_disabled_provider(void) {
    AppData app;
    reset_state(&app);
    app.window_visible = TRUE;
    workspaces_enabled = 0;

    dispatch_hotkey_mode(&app, SHOW_MODE_WORKSPACES);

    ASSERT_TRUE("disabled visible workspaces hotkey does not surface tab",
                surface_tab_calls == 0);
    ASSERT_TRUE("disabled visible workspaces hotkey does not grab focus",
                grab_focus_calls == 0);
    ASSERT_TRUE("disabled visible workspaces hotkey keeps current tab",
                app.current_tab == TAB_WINDOWS);
}

static void test_disabled_visible_workspaces_preserves_command_state(void) {
    AppData app;
    reset_state(&app);
    app.window_visible = TRUE;
    app.command_mode.state = CMD_MODE_COMMAND;
    workspaces_enabled = 0;

    dispatch_hotkey_mode(&app, SHOW_MODE_WORKSPACES);

    ASSERT_TRUE("disabled workspaces keeps command state",
                app.command_mode.state == CMD_MODE_COMMAND);
    ASSERT_TRUE("disabled workspaces does not surface from command state",
                surface_tab_calls == 0);
}

static void test_disabled_visible_run_preserves_modal_state(void) {
    AppData app;
    reset_state(&app);
    app.window_visible = TRUE;
    app.command_mode.state = CMD_MODE_MODAL;
    run_enabled = 0;

    dispatch_hotkey_mode(&app, SHOW_MODE_RUN);

    ASSERT_TRUE("disabled run keeps modal state",
                app.command_mode.state == CMD_MODE_MODAL);
    ASSERT_TRUE("disabled run does not enter modal again",
                enter_modal_calls == 0);
}

int main(void) {
    printf("Hotkey dispatch tests\n");
    printf("=====================\n\n");

    test_hidden_workspaces_hotkey_surfaces_enabled_provider();
    test_hidden_harpoon_hotkey_surfaces_enabled_provider();
    test_visible_harpoon_hotkey_surfaces_enabled_provider();
    test_hidden_harpoon_hotkey_ignores_disabled_provider();
    test_hidden_workspaces_hotkey_ignores_disabled_provider();
    test_visible_workspaces_hotkey_ignores_disabled_provider();
    test_disabled_visible_workspaces_preserves_command_state();
    test_disabled_visible_run_preserves_modal_state();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
