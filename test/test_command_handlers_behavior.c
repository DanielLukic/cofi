#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/command_definitions.h"
#include "../src/tiling.h"

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

static int show_name_assign_overlay_calls = 0;
static int dispatch_hotkey_mode_calls = 0;
static ShowMode last_show_mode = SHOW_MODE_WINDOWS;

// --- shared stubs for handler dependencies ---
void hide_window(AppData *app) { (void)app; }
void dispatch_hotkey_mode(AppData *app, ShowMode mode) {
    (void)app;
    dispatch_hotkey_mode_calls++;
    last_show_mode = mode;
}
void exit_command_mode(AppData *app) { (void)app; }
void show_help_commands(AppData *app) { (void)app; }
void switch_to_tab(AppData *app, TabMode target_tab) { (void)app; (void)target_tab; }
void surface_tab(AppData *app, TabMode tab) { (void)app; (void)tab; }

int get_current_desktop(Display *display) { (void)display; return 0; }
int resolve_workspace_from_arg(Display *display, const char *arg, int workspaces_per_row) {
    (void)display; (void)arg; (void)workspaces_per_row;
    return -1;
}
int get_number_of_desktops(Display *display) { (void)display; return 4; }
void switch_to_desktop(Display *display, int desktop) { (void)display; (void)desktop; }
void move_window_to_desktop(Display *display, Window window, int desktop) {
    (void)display; (void)window; (void)desktop;
}

void move_window_to_next_monitor(AppData *app) { (void)app; }
void show_workspace_jump_overlay(AppData *app) { (void)app; }
void show_workspace_rename_overlay(AppData *app, int workspace_index) {
    (void)app; (void)workspace_index;
}
void show_workspace_move_all_overlay(AppData *app) { (void)app; }
void show_tiling_overlay(AppData *app) { (void)app; }
void show_name_assign_overlay(AppData *app) {
    (void)app;
    show_name_assign_overlay_calls++;
}

void assign_workspace_slots(AppData *app) { (void)app; }
void apply_tiling(Display *display, Window window, TileOption option, int columns) {
    (void)display; (void)window; (void)option; (void)columns;
}
void toggle_window_state(Display *display, Window window, const char *state) {
    (void)display; (void)window; (void)state;
}
void close_window(Display *display, Window window) { (void)display; (void)window; }
void minimize_window(Display *display, Window window) { (void)display; (void)window; }
void activate_window(Display *display, Window window) { (void)display; (void)window; }
gboolean get_window_state(Display *display, Window window, const char *state_name) {
    (void)display; (void)window; (void)state_name;
    return FALSE;
}
gboolean get_window_geometry(Display *display, Window window, int *x, int *y, int *w, int *h) {
    (void)display; (void)window;
    if (x) *x = 0;
    if (y) *y = 0;
    if (w) *w = 100;
    if (h) *h = 100;
    return TRUE;
}

void save_config(const CofiConfig *config) { (void)config; }
int apply_config_setting(CofiConfig *config, const char *key, const char *value,
                         char *error, size_t error_size) {
    (void)config; (void)key; (void)value; (void)error; (void)error_size;
    return 0;
}
int parse_hotkey_command(const char *args, char *key, size_t key_size,
                         char *cmd, size_t cmd_size) {
    (void)args; (void)key; (void)key_size; (void)cmd; (void)cmd_size;
    return 0;
}
int add_hotkey_binding(HotkeyConfig *config, const char *key, const char *command) {
    (void)config; (void)key; (void)command;
    return 1;
}
gboolean remove_hotkey_binding(HotkeyConfig *config, const char *key) {
    (void)config; (void)key;
    return FALSE;
}
gboolean save_hotkey_config(const HotkeyConfig *config) { (void)config; return TRUE; }
void regrab_hotkeys(AppData *app) { (void)app; }
int format_hotkey_display(const HotkeyConfig *config, char *buffer, size_t size) {
    (void)config;
    if (size > 0) buffer[0] = '\0';
    return 0;
}

static const CommandDef *find_command(const char *primary) {
    for (int i = 0; COMMAND_DEFINITIONS[i].primary; i++) {
        if (strcmp(COMMAND_DEFINITIONS[i].primary, primary) == 0) {
            return &COMMAND_DEFINITIONS[i];
        }
    }
    return NULL;
}

static void test_window_handler_behavior(void) {
    AppData app;
    WindowInfo window;
    memset(&app, 0, sizeof(app));
    memset(&window, 0, sizeof(window));

    const CommandDef *cmd = find_command("an");
    ASSERT_TRUE("window command 'an' exists", cmd != NULL);

    if (!cmd) return;

    app.current_tab = TAB_HOTKEYS;
    gboolean no_window_result = cmd->handler(&app, NULL, "");
    ASSERT_TRUE("an returns TRUE on missing window", no_window_result == TRUE);

    gboolean wrong_tab_result = cmd->handler(&app, &window, "");
    ASSERT_TRUE("an returns TRUE outside windows tab", wrong_tab_result == TRUE);

    app.current_tab = TAB_WINDOWS;
    show_name_assign_overlay_calls = 0;
    gboolean open_overlay_result = cmd->handler(&app, &window, "");
    ASSERT_TRUE("an returns FALSE when opening overlay", open_overlay_result == FALSE);
    ASSERT_TRUE("an opens name assign overlay", show_name_assign_overlay_calls == 1);
}

static void test_workspace_handler_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    const CommandDef *cmd = find_command("rw");
    ASSERT_TRUE("workspace command 'rw' exists", cmd != NULL);
    if (!cmd) return;

    gboolean invalid_workspace = cmd->handler(&app, NULL, "0");
    ASSERT_TRUE("rw rejects workspace 0", invalid_workspace == FALSE);
}

static void test_tiling_handler_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    const CommandDef *cmd = find_command("mouse");
    ASSERT_TRUE("tiling command 'mouse' exists", cmd != NULL);
    if (!cmd) return;

    gboolean result = cmd->handler(&app, NULL, "away");
    ASSERT_TRUE("mouse fails when display is unavailable", result == FALSE);
}

static void test_ui_handler_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    const CommandDef *cmd = find_command("show");
    ASSERT_TRUE("ui command 'show' exists", cmd != NULL);
    if (!cmd) return;

    gboolean result = cmd->handler(&app, NULL, "not-a-mode");
    ASSERT_TRUE("show rejects invalid mode", result == FALSE);
    ASSERT_TRUE("show invalid mode sets showing_help", app.command_mode.showing_help == TRUE);

    dispatch_hotkey_mode_calls = 0;
    last_show_mode = SHOW_MODE_WINDOWS;
    result = cmd->handler(&app, NULL, "run");
    ASSERT_TRUE("show run is accepted", result == FALSE);
    ASSERT_TRUE("show run dispatches exactly once", dispatch_hotkey_mode_calls == 1);
    ASSERT_TRUE("show run dispatches run mode", last_show_mode == SHOW_MODE_RUN);
}

int main(void) {
    printf("Command handler behavior regression tests\n");
    printf("========================================\n\n");

    test_window_handler_behavior();
    test_workspace_handler_behavior();
    test_tiling_handler_behavior();
    test_ui_handler_behavior();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
