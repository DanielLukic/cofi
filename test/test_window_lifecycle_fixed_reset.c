#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

void save_config(const CofiConfig *config) { (void)config; }
void save_harpoon_slots(const HarpoonManager *harpoon) { (void)harpoon; }
void reset_selection(AppData *app) { (void)app; }
void exit_command_mode(AppData *app) { (void)app; }
void cofi_exit_modal(AppData *app) { (void)app; }
void clear_surfaced_tabs(AppData *app) { (void)app; }
void hide_overlay(AppData *app) { (void)app; }
void get_window_list(AppData *app) { (void)app; }
bool match_entry_reassign_live_windows(MatchEntryManager *manager, WindowInfo *windows, int window_count)
    { (void)manager; (void)windows; (void)window_count; return false; }
void save_match_entries(const MatchEntryManager *manager) { (void)manager; }
void filter_windows(AppData *app, const char *query) { (void)app; (void)query; }
void filter_workspaces(AppData *app, const char *query) { (void)app; (void)query; }
static int provider_on_leave_calls = 0;
static CofiTabProvider test_provider;
static int test_provider_tab_mode = -1;
static void test_provider_on_leave(AppData *app) { (void)app; provider_on_leave_calls++; }
const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    if (tab_mode == test_provider_tab_mode) return &test_provider;
    return NULL;
}
void update_display(AppData *app) { (void)app; }
static int init_fixed_window_size_calls = 0;
void init_fixed_window_size(AppData *app) { (void)app; init_fixed_window_size_calls++; }
int get_current_desktop(Display *display) { (void)display; return -1; }
CofiResult get_x11_property(Display *display, Window window, Atom property, Atom req_type,
                            unsigned long long_length, Atom *actual_type, int *actual_format,
                            unsigned long *n_items, unsigned char **prop)
    { (void)display; (void)window; (void)property; (void)req_type; (void)long_length;
      (void)actual_type; (void)actual_format; (void)n_items; (void)prop; return COFI_ERROR; }
void move_window_to_desktop(Display *display, Window window, int desktop)
    { (void)display; (void)window; (void)desktop; }
void destroy_slot_overlays(AppData *app) { (void)app; }
void assign_workspace_slots(AppData *app) { (void)app; }
void log_debug(const char *fmt, ...) { (void)fmt; }
void log_info(const char *fmt, ...) { (void)fmt; }
void log_error(const char *fmt, ...) { (void)fmt; }
void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

#include "ui/window_lifecycle.c"

static void test_show_window_reinitializes_fixed_window_cache(void) {
    AppData app = {0};

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app.entry = gtk_entry_new();
    app.window_visible = FALSE;
    app.current_tab = TAB_WINDOWS;
    app.fixed_cols = 120;
    app.fixed_rows = 26;

    show_window(&app);

    ASSERT_TRUE("show_window invalidates fixed_cols before resize authority",
                app.fixed_cols == 0);
    ASSERT_TRUE("show_window invalidates fixed_rows before resize authority",
                app.fixed_rows == 0);
    ASSERT_TRUE("show_window recomputes fixed sizing every show",
                init_fixed_window_size_calls == 1);

    gtk_widget_destroy(app.window);
}

static void test_hide_window_notifies_provider_on_leave(void) {
    AppData app = {0};

    test_provider_tab_mode = TAB_COUNT + 42;
    memset(&test_provider, 0, sizeof(test_provider));
    test_provider.tab_mode = test_provider_tab_mode;
    test_provider.on_leave = test_provider_on_leave;
    provider_on_leave_calls = 0;

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app.entry = gtk_entry_new();
    app.window_visible = TRUE;
    app.current_tab = (TabMode)test_provider_tab_mode;

    hide_window(&app);

    ASSERT_TRUE("hide_window calls provider on_leave for active provider tab",
                provider_on_leave_calls == 1);

    gtk_widget_destroy(app.window);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Window lifecycle fixed-size reset tests\n");
        printf("======================================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Window lifecycle fixed-size reset tests\n");
    printf("======================================\n\n");

    test_show_window_reinitializes_fixed_window_cache();
    test_hide_window_notifies_provider_on_leave();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
