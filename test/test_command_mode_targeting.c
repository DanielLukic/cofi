#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/command_api.h"
#include "../src/command_mode.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_update_display_calls = 0;

void hide_window(AppData *app) { (void)app; }
void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}
void enter_run_mode(AppData *app, const char *cmd) { (void)app; (void)cmd; }
void move_selection_up(AppData *app) { (void)app; }
void move_selection_down(AppData *app) { (void)app; }
gboolean execute_command(const char *cmd, AppData *app) { (void)cmd; (void)app; return TRUE; }
char *generate_command_help_text(HelpFormat fmt) { (void)fmt; return NULL; }
int get_display_columns(AppData *app) { (void)app; return 80; }
int get_max_display_lines_dynamic(AppData *app) { (void)app; return 20; }
void overlay_scrollbar(GString *s, int t, int v, int o, int c)
    { (void)s; (void)t; (void)v; (void)o; (void)c; }

#include "../src/command_parser.c"
#include "../src/command_mode.c"

static GtkWidget *g_fake_entry = NULL;
static GtkWidget *g_fake_label = NULL;

static AppData make_app_with_windows(int count) {
    AppData app = {0};
    app.current_tab = TAB_WINDOWS;
    app.filtered_count = count;
    for (int i = 0; i < count; i++) {
        app.filtered[i].id = (Window)(0x100 + i);
    }
    app.selection.window_index = 0;
    app.selection.selected_window_id = app.filtered[0].id;
    app.command_target_id = 0;
    app.entry = g_fake_entry;
    app.mode_indicator = g_fake_label;
    init_command_mode(&app.command_mode);
    return app;
}

static void test_command_target_id_selects_matching_window(void) {
    AppData app = make_app_with_windows(5);
    app.command_target_id = (Window)0x102; // index 2

    enter_command_mode(&app);

    ASSERT_TRUE("target window at index 2 is selected",
                app.selection.window_index == 2);
    ASSERT_TRUE("selected_window_id matches target",
                app.selection.selected_window_id == (Window)0x102);
}

static void test_zero_command_target_id_leaves_selection_unchanged(void) {
    AppData app = make_app_with_windows(5);
    app.selection.window_index = 3;
    app.selection.selected_window_id = app.filtered[3].id;
    app.command_target_id = 0;

    enter_command_mode(&app);

    ASSERT_TRUE("selection index unchanged when command_target_id is 0",
                app.selection.window_index == 3);
    ASSERT_TRUE("selected_window_id unchanged when command_target_id is 0",
                app.selection.selected_window_id == (Window)0x103);
}

static void test_unmatched_command_target_id_leaves_selection_unchanged(void) {
    AppData app = make_app_with_windows(5);
    app.selection.window_index = 1;
    app.selection.selected_window_id = app.filtered[1].id;
    app.command_target_id = (Window)0xdeadbeef; // not in filtered

    enter_command_mode(&app);

    ASSERT_TRUE("selection index unchanged when target not in filtered list",
                app.selection.window_index == 1);
    ASSERT_TRUE("selected_window_id unchanged when target not in filtered list",
                app.selection.selected_window_id == (Window)0x101);
}

static void test_exit_command_mode_resets_command_target_id(void) {
    AppData app = make_app_with_windows(3);
    app.command_target_id = (Window)0x101;
    enter_command_mode(&app);
    exit_command_mode(&app);

    ASSERT_TRUE("command_target_id reset to 0 on exit",
                app.command_target_id == 0);
}

static void test_exit_command_mode_is_noop_when_already_normal(void) {
    AppData app = make_app_with_windows(4);
    int before_filtered_count;
    int before_window_index;
    Window before_selected_window_id;

    app.command_mode.state = CMD_MODE_NORMAL;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "obs");
    app.selection.window_index = 2;
    app.selection.selected_window_id = app.filtered[2].id;

    before_filtered_count = app.filtered_count;
    before_window_index = app.selection.window_index;
    before_selected_window_id = app.selection.selected_window_id;
    g_update_display_calls = 0;

    exit_command_mode(&app);

    ASSERT_TRUE("exit command mode keeps entry text in normal mode",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "obs") == 0);
    ASSERT_TRUE("exit command mode keeps filtered_count in normal mode",
                app.filtered_count == before_filtered_count);
    ASSERT_TRUE("exit command mode keeps selection index in normal mode",
                app.selection.window_index == before_window_index);
    ASSERT_TRUE("exit command mode keeps selected window id in normal mode",
                app.selection.selected_window_id == before_selected_window_id);
    ASSERT_TRUE("exit command mode keeps mode NORMAL",
                app.command_mode.state == CMD_MODE_NORMAL);
    ASSERT_TRUE("exit command mode does not update display in normal mode",
                g_update_display_calls == 0);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Command mode targeting tests\n");
        printf("============================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }
    g_fake_entry = gtk_entry_new();
    g_fake_label = gtk_label_new(">");

    test_command_target_id_selects_matching_window();
    test_zero_command_target_id_leaves_selection_unchanged();
    test_unmatched_command_target_id_leaves_selection_unchanged();
    test_exit_command_mode_resets_command_target_id();
    test_exit_command_mode_is_noop_when_already_normal();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
