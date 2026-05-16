#include <stdio.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/window_lifecycle.h"

static int pass = 0;
static int fail = 0;
static int assign_workspace_slots_calls = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

void assign_workspace_slots(AppData *app) {
    (void)app;
    assign_workspace_slots_calls++;
}

void destroy_slot_overlays(AppData *app) { (void)app; }
void hide_overlay(AppData *app) { (void)app; }
void clear_surfaced_tabs(AppData *app) { (void)app; }
void reset_selection(AppData *app) { (void)app; }
void get_window_list(AppData *app) { (void)app; }
bool check_and_reassign_windows(HarpoonManager *harpoon, WindowInfo *windows, int window_count)
    { (void)harpoon; (void)windows; (void)window_count; return false; }
void filter_windows(AppData *app, const char *query) { (void)app; (void)query; }
void filter_workspaces(AppData *app, const char *query) { (void)app; (void)query; }
const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) { (void)tab_mode; return NULL; }
void update_display(AppData *app) { (void)app; }
int get_current_desktop(Display *display) { (void)display; return -1; }
CofiResult get_x11_property(Display *display, Window window, Atom property, Atom expected_type,
                            unsigned long long_length, Atom *actual_type, int *actual_format,
                            unsigned long *n_items, unsigned char **prop)
    { (void)display; (void)window; (void)property; (void)expected_type; (void)long_length;
      (void)actual_type; (void)actual_format; (void)n_items; (void)prop; return COFI_ERROR; }
void move_window_to_desktop(Display *display, Window window, int desktop)
    { (void)display; (void)window; (void)desktop; }
void exit_command_mode(AppData *app) { (void)app; }
void cofi_exit_modal(AppData *app) { (void)app; }
void save_config(const CofiConfig *config) { (void)config; }
void save_harpoon_slots(const HarpoonManager *harpoon) { (void)harpoon; }
void init_fixed_window_size(AppData *app) { (void)app; }
void log_debug(const char *fmt, ...) { (void)fmt; }
void log_info(const char *fmt, ...) { (void)fmt; }
void log_error(const char *fmt, ...) { (void)fmt; }
void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

#include "../src/window_lifecycle.c"

static AppData make_app(DigitSlotMode mode, TabMode tab, gboolean visible) {
    AppData app = {0};
    app.config.digit_slot_mode = mode;
    app.current_tab = tab;
    app.window_visible = visible;
    return app;
}

static void test_calls_assign_for_workspace_windows_visible(void) {
    AppData app = make_app(DIGIT_MODE_PER_WORKSPACE, TAB_WINDOWS, TRUE);
    assign_workspace_slots_calls = 0;

    maybe_show_initial_slot_overlays(&app);

    ASSERT_TRUE("assign called for per-workspace windows tab while visible",
                assign_workspace_slots_calls == 1);
}

static void test_does_not_call_for_default_mode(void) {
    AppData app = make_app(DIGIT_MODE_DEFAULT, TAB_WINDOWS, TRUE);
    assign_workspace_slots_calls = 0;

    maybe_show_initial_slot_overlays(&app);

    ASSERT_TRUE("assign not called in default digit mode",
                assign_workspace_slots_calls == 0);
}

static void test_does_not_call_for_workspaces_mode(void) {
    AppData app = make_app(DIGIT_MODE_WORKSPACES, TAB_WINDOWS, TRUE);
    assign_workspace_slots_calls = 0;

    maybe_show_initial_slot_overlays(&app);

    ASSERT_TRUE("assign not called in workspaces digit mode",
                assign_workspace_slots_calls == 0);
}

static void test_does_not_call_for_non_windows_tabs(void) {
    AppData app_harpoon = make_app(DIGIT_MODE_PER_WORKSPACE, TAB_HARPOON, TRUE);
    AppData app_apps = make_app(DIGIT_MODE_PER_WORKSPACE, TAB_APPS, TRUE);
    assign_workspace_slots_calls = 0;

    maybe_show_initial_slot_overlays(&app_harpoon);
    maybe_show_initial_slot_overlays(&app_apps);

    ASSERT_TRUE("assign not called for non-windows tabs",
                assign_workspace_slots_calls == 0);
}

static void test_does_not_call_when_not_visible(void) {
    AppData app = make_app(DIGIT_MODE_PER_WORKSPACE, TAB_WINDOWS, FALSE);
    assign_workspace_slots_calls = 0;

    maybe_show_initial_slot_overlays(&app);

    ASSERT_TRUE("assign not called when window is hidden",
                assign_workspace_slots_calls == 0);
}

int main(void) {
    printf("Initial slot overlays tests\n");
    printf("===========================\n\n");

    test_calls_assign_for_workspace_windows_visible();
    test_does_not_call_for_default_mode();
    test_does_not_call_for_workspaces_mode();
    test_does_not_call_for_non_windows_tabs();
    test_does_not_call_when_not_visible();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
