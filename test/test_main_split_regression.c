#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/config.h"

void filter_config(AppData *app, const char *filter);
void filter_hotkeys(AppData *app, const char *filter);
void hide_window(AppData *app);
void dispatch_hotkey_mode(AppData *app, ShowMode mode);
gboolean handle_command_key(GdkEventKey *event, AppData *app);

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

static void fill_window(WindowInfo *win, Window id, const char *title, const char *class_name) {
    memset(win, 0, sizeof(*win));
    win->id = id;
    strncpy(win->title, title, sizeof(win->title) - 1);
    strncpy(win->class_name, class_name, sizeof(win->class_name) - 1);
    strncpy(win->instance, class_name, sizeof(win->instance) - 1);
    strncpy(win->type, "Normal", sizeof(win->type) - 1);
}

static void test_filter_hotkeys_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    app.hotkey_config.count = 3;
    strcpy(app.hotkey_config.bindings[0].key, "Mod4+w");
    strcpy(app.hotkey_config.bindings[0].command, "show windows");
    strcpy(app.hotkey_config.bindings[1].key, "Mod1+Tab");
    strcpy(app.hotkey_config.bindings[1].command, "show command");
    strcpy(app.hotkey_config.bindings[2].key, "Control+j");
    strcpy(app.hotkey_config.bindings[2].command, "jw");

    filter_hotkeys(&app, "");
    ASSERT_TRUE("filter_hotkeys empty filter keeps all bindings", app.filtered_hotkeys_count == 3);

    filter_hotkeys(&app, "Mod1");
    ASSERT_TRUE("filter_hotkeys narrows to matching key", app.filtered_hotkeys_count == 1);
    ASSERT_TRUE("filter_hotkeys preserves matching binding", strcmp(app.filtered_hotkeys[0].key, "Mod1+Tab") == 0);
}

static void test_filter_config_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    init_config_defaults(&app.config);

    filter_config(&app, "");
    ASSERT_TRUE("filter_config empty filter yields entries", app.filtered_config_count > 0);

    filter_config(&app, "tile_columns");
    ASSERT_TRUE("filter_config key search yields one entry", app.filtered_config_count == 1);
    ASSERT_TRUE("filter_config matched key is tile_columns",
                strcmp(app.filtered_config[0].key, "tile_columns") == 0);
}

static void test_hide_window_noop_when_already_hidden(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    app.window_visible = FALSE;
    app.command_mode.state = CMD_MODE_COMMAND;

    hide_window(&app);

    ASSERT_TRUE("hide_window keeps command state unchanged when already hidden",
                app.command_mode.state == CMD_MODE_COMMAND);
}

static void test_dispatch_hotkey_mode_steps_windows_selection(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    app.window_visible = TRUE;
    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 2;
    app.selection.window_index = 0;
    fill_window(&app.filtered[0], 0x100, "First", "ClassA");
    fill_window(&app.filtered[1], 0x200, "Second", "ClassB");
    app.textbuffer = gtk_text_buffer_new(NULL);

    dispatch_hotkey_mode(&app, SHOW_MODE_WINDOWS);

    ASSERT_TRUE("dispatch_hotkey_mode SHOW_MODE_WINDOWS steps selection when already visible",
                app.selection.window_index == 1);
}

static void test_exclam_switches_from_command_mode_to_run_mode(void) {
    AppData app;
    GdkEventKey event;
    memset(&app, 0, sizeof(app));
    memset(&event, 0, sizeof(event));

    app.command_mode.state = CMD_MODE_COMMAND;
    app.entry = gtk_entry_new();
    app.mode_indicator = gtk_label_new(":");

    gtk_entry_set_text(GTK_ENTRY(app.entry), "");
    event.keyval = GDK_KEY_exclam;

    ASSERT_TRUE("handle_command_key consumes ! in command mode",
                handle_command_key(&event, &app) == TRUE);
    ASSERT_TRUE("! switches entry state to run mode",
                app.command_mode.state == CMD_MODE_RUN);
    ASSERT_TRUE("! seeds run prompt in entry",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "!") == 0);
}

int main(void) {
    int argc = 0;
    char **argv = NULL;

    if (!gtk_init_check(&argc, &argv)) {
        printf("Main split regression tests\n");
        printf("===========================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Main split regression tests\n");
    printf("===========================\n\n");

    test_filter_hotkeys_behavior();
    test_filter_config_behavior();
    test_hide_window_noop_when_already_hidden();
    test_dispatch_hotkey_mode_steps_windows_selection();
    test_exclam_switches_from_command_mode_to_run_mode();

    printf("\n===========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
