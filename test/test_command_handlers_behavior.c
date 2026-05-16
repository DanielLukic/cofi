#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/command_definitions.h"
#include "../src/tiling.h"
#include "../src/x11_utils.h"

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
static int set_window_state_calls = 0;
static WindowStateAction last_window_state_action = WINDOW_STATE_TOGGLE;
static Window last_window_state_window = 0;
static char last_window_state_name[64] = {0};
static int parse_hotkey_action = 0;
static int hide_window_calls = 0;
static int g_assign_workspace_slots_calls = 0;
static int g_get_workspace_slot_calls = 0;
static int g_last_workspace_slot = 0;
static Window g_workspace_slot_target = 0;
static int g_get_window_list_calls = 0;
static int g_set_workspace_switch_state_calls = 0;
static int g_last_workspace_switch_state = -1;
static int g_highlight_calls = 0;
static Window g_last_highlight_window = 0;
static int g_activate_calls = 0;
static Window g_last_activate_window = 0;

// --- shared stubs for handler dependencies ---
void hide_window(AppData *app) {
    (void)app;
    hide_window_calls++;
}
void dispatch_hotkey_mode(AppData *app, ShowMode mode) {
    (void)app;
    dispatch_hotkey_mode_calls++;
    last_show_mode = mode;
}
void exit_command_mode(AppData *app) { (void)app; }
void show_help_commands(AppData *app) { (void)app; }
void switch_to_tab(AppData *app, TabMode target_tab) { (void)app; (void)target_tab; }
void surface_tab(AppData *app, TabMode tab) { if (app) app->current_tab = tab; }
static int g_enter_modal_calls_cmd = 0;
static CofiTabProvider g_stub_run_provider;
static CofiTabProvider g_stub_calc_provider;
static CofiTabProvider g_stub_profiles_provider;
static int g_cmd_args_calls = 0;
static char g_cmd_args_last[256] = {0};
static CofiActionStatus g_cmd_args_result = COFI_HANDLED_HIDE;

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    (void)provider;
    g_enter_modal_calls_cmd++;
    if (app) app->command_mode.state = CMD_MODE_MODAL;
}
void cofi_exit_modal(AppData *app) { (void)app; }
const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) {
    if (prefix == '!') return &g_stub_run_provider;
    return NULL;
}
const CofiTabProvider *cofi_get_provider_for_command(const char *command) {
    if (command && strcmp(command, "calc") == 0) return &g_stub_calc_provider;
    if (command && strcmp(command, "profiles") == 0) return &g_stub_profiles_provider;
    return NULL;
}
int cofi_get_provider_id_for_tab(int tab_mode) { (void)tab_mode; return -1; }
CofiActionStatus cofi_call_on_command_args(int provider_id, AppData *app, const char *args) {
    (void)provider_id; (void)app;
    if (args && args[0] != '\0') {
        g_cmd_args_calls++;
        g_strlcpy(g_cmd_args_last, args, sizeof(g_cmd_args_last));
        return g_cmd_args_result;
    }
    return COFI_NO_OP;
}

static int g_detach_launch_shell_calls = 0;
static char g_last_launched_command[256] = {0};
static gboolean g_detach_launch_shell_result = TRUE;

gboolean detach_launch_shell(const char *command) {
    g_detach_launch_shell_calls++;
    if (command) g_strlcpy(g_last_launched_command, command, sizeof(g_last_launched_command));
    return g_detach_launch_shell_result;
}

static int g_add_run_history_calls = 0;
void add_run_history_entry(RunMode *run_mode, const char *command) {
    (void)run_mode; (void)command;
    g_add_run_history_calls++;
}

gboolean extract_run_command(const char *entry_text, char *command_out, size_t command_size) {
    if (!entry_text || !command_out || command_size == 0) return FALSE;
    g_strlcpy(command_out, entry_text, command_size);
    return command_out[0] != '\0';
}

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

void assign_workspace_slots(AppData *app) {
    (void)app;
    g_assign_workspace_slots_calls++;
}
Window get_workspace_slot_window(const WorkspaceSlotManager *manager, int slot) {
    (void)manager;
    g_get_workspace_slot_calls++;
    g_last_workspace_slot = slot;
    return g_workspace_slot_target;
}
void get_window_list(AppData *app) {
    (void)app;
    g_get_window_list_calls++;
}
void set_workspace_switch_state(int suppress) {
    g_set_workspace_switch_state_calls++;
    g_last_workspace_switch_state = suppress;
}
void highlight_window(AppData *app, Window target) {
    (void)app;
    g_highlight_calls++;
    g_last_highlight_window = target;
}
void apply_tiling(Display *display, Window window, TileOption option, int columns) {
    (void)display; (void)window; (void)option; (void)columns;
}
void set_window_state(Display *display, Window window, const char *state,
                      WindowStateAction action) {
    (void)display;
    set_window_state_calls++;
    last_window_state_action = action;
    last_window_state_window = window;
    strncpy(last_window_state_name, state ? state : "", sizeof(last_window_state_name) - 1);
    last_window_state_name[sizeof(last_window_state_name) - 1] = '\0';
}

void toggle_window_state(Display *display, Window window, const char *state) {
    set_window_state(display, window, state, WINDOW_STATE_TOGGLE);
}
void close_window(Display *display, Window window) { (void)display; (void)window; }
void minimize_window(Display *display, Window window) { (void)display; (void)window; }
void activate_window(Display *display, Window window) {
    (void)display;
    g_activate_calls++;
    g_last_activate_window = window;
}
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
    return parse_hotkey_action;
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

static void test_window_state_handler(const char *cmd_name, const char *atom_name) {
    AppData app;
    WindowInfo window;
    memset(&app, 0, sizeof(app));
    memset(&window, 0, sizeof(window));
    window.id = 0xBEEF;

    const CommandDef *cmd = find_command(cmd_name);
    ASSERT_TRUE("window state command exists", cmd != NULL);
    if (!cmd) return;

    set_window_state_calls = 0;
    gboolean missing_window = cmd->handler(&app, NULL, "on");
    ASSERT_TRUE("state command rejects missing window", missing_window == FALSE);
    ASSERT_TRUE("missing window does not touch state", set_window_state_calls == 0);

    set_window_state_calls = 0;
    ASSERT_TRUE("no-arg succeeds", cmd->handler(&app, &window, "") == TRUE);
    ASSERT_TRUE("no-arg uses toggle", set_window_state_calls == 1 && last_window_state_action == WINDOW_STATE_TOGGLE);

    set_window_state_calls = 0;
    ASSERT_TRUE("toggle succeeds", cmd->handler(&app, &window, "toggle") == TRUE);
    ASSERT_TRUE("toggle uses toggle action", set_window_state_calls == 1 && last_window_state_action == WINDOW_STATE_TOGGLE);

    set_window_state_calls = 0;
    ASSERT_TRUE("on succeeds", cmd->handler(&app, &window, "on") == TRUE);
    ASSERT_TRUE("on uses set action", set_window_state_calls == 1 && last_window_state_action == WINDOW_STATE_SET);

    set_window_state_calls = 0;
    ASSERT_TRUE("off succeeds", cmd->handler(&app, &window, "off") == TRUE);
    ASSERT_TRUE("off uses unset action", set_window_state_calls == 1 && last_window_state_action == WINDOW_STATE_UNSET);

    set_window_state_calls = 0;
    ASSERT_TRUE("compact + succeeds", cmd->handler(&app, &window, "+") == TRUE);
    ASSERT_TRUE("compact + uses set action", set_window_state_calls == 1 && last_window_state_action == WINDOW_STATE_SET);

    set_window_state_calls = 0;
    ASSERT_TRUE("compact - succeeds", cmd->handler(&app, &window, "-") == TRUE);
    ASSERT_TRUE("compact - uses unset action", set_window_state_calls == 1 && last_window_state_action == WINDOW_STATE_UNSET);

    set_window_state_calls = 0;
    ASSERT_TRUE("invalid arg fails", cmd->handler(&app, &window, "wat") == FALSE);
    ASSERT_TRUE("invalid arg is no-op", set_window_state_calls == 0);

    ASSERT_TRUE("targets expected atom", strcmp(last_window_state_name, atom_name) == 0);
    ASSERT_TRUE("targets selected window", last_window_state_window == window.id);
}

static void test_window_state_handlers_behavior(void) {
    test_window_state_handler("sb", "_NET_WM_STATE_SKIP_TASKBAR");
    test_window_state_handler("ab", "_NET_WM_STATE_BELOW");
    test_window_state_handler("aot", "_NET_WM_STATE_ABOVE");
    test_window_state_handler("ew", "_NET_WM_STATE_STICKY");
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

static void test_jump_slot_handler_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.display = (Display *)0x1;

    const CommandDef *cmd = find_command("jump-slot");
    ASSERT_TRUE("jump-slot command exists", cmd != NULL);
    if (!cmd) return;

    g_assign_workspace_slots_calls = 0;
    g_get_workspace_slot_calls = 0;
    g_get_window_list_calls = 0;
    g_set_workspace_switch_state_calls = 0;
    g_highlight_calls = 0;
    g_activate_calls = 0;
    hide_window_calls = 0;
    g_workspace_slot_target = 0;

    ASSERT_TRUE("jump-slot rejects missing arg", cmd->handler(&app, NULL, "") == FALSE);
    ASSERT_TRUE("jump-slot rejects zero", cmd->handler(&app, NULL, "0") == FALSE);
    ASSERT_TRUE("jump-slot rejects ten", cmd->handler(&app, NULL, "10") == FALSE);
    ASSERT_TRUE("jump-slot rejects alpha", cmd->handler(&app, NULL, "abc") == FALSE);
    ASSERT_TRUE("jump-slot rejects trailing garbage", cmd->handler(&app, NULL, "1x") == FALSE);
    ASSERT_TRUE("invalid args do not dispatch workspace slots",
                g_assign_workspace_slots_calls == 0 && g_get_workspace_slot_calls == 0);

    g_workspace_slot_target = (Window)0xBEEF;
    ASSERT_TRUE("jump-slot valid arg succeeds", cmd->handler(&app, NULL, "1") == TRUE);
    ASSERT_TRUE("jump-slot refreshes window list first", g_get_window_list_calls == 1);
    ASSERT_TRUE("jump-slot assigns workspace slots", g_assign_workspace_slots_calls == 1);
    ASSERT_TRUE("jump-slot queries requested slot", g_get_workspace_slot_calls == 1 && g_last_workspace_slot == 1);
    ASSERT_TRUE("jump-slot toggles workspace-switch suppress", g_set_workspace_switch_state_calls == 1 && g_last_workspace_switch_state == 1);
    ASSERT_TRUE("jump-slot activates target window",
                g_activate_calls == 1 && g_last_activate_window == (Window)0xBEEF);
    ASSERT_TRUE("jump-slot highlights target window", g_highlight_calls == 1 && g_last_highlight_window == (Window)0xBEEF);
    ASSERT_TRUE("jump-slot hides launcher window", hide_window_calls == 1);

    g_workspace_slot_target = 0;
    ASSERT_TRUE("jump-slot missing slot target fails", cmd->handler(&app, NULL, "2") == FALSE);
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

    result = cmd->handler(&app, NULL, "rules");
    ASSERT_TRUE("show rules is accepted", result == FALSE);
    ASSERT_TRUE("show rules surfaces rules tab", app.current_tab == TAB_RULES);

    cmd = find_command("rules");
    ASSERT_TRUE("rules command exists", cmd != NULL);
    if (cmd) {
        result = cmd->handler(&app, NULL, "");
        ASSERT_TRUE("rules command surfaces rules tab", result == FALSE && app.current_tab == TAB_RULES);
    }

    cmd = find_command("hotkeys");
    ASSERT_TRUE("hotkeys command exists", cmd != NULL);
    if (cmd) {
        parse_hotkey_action = 0;
        app.current_tab = TAB_WINDOWS;
        result = cmd->handler(&app, NULL, "");
        ASSERT_TRUE("hotkeys bare surfaces hotkeys tab", result == FALSE && app.current_tab == TAB_HOTKEYS);

        parse_hotkey_action = 1;
        app.current_tab = TAB_WINDOWS;
        result = cmd->handler(&app, NULL, "bind");
        ASSERT_TRUE("hotkeys mutation surfaces hotkeys tab", result == FALSE && app.current_tab == TAB_HOTKEYS);

        parse_hotkey_action = 2;
        app.current_tab = TAB_WINDOWS;
        result = cmd->handler(&app, NULL, "unbind");
        ASSERT_TRUE("hotkeys remove surfaces hotkeys tab", result == FALSE && app.current_tab == TAB_HOTKEYS);
    }
}

static void test_cmd_run_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    const CommandDef *cmd = find_command("run");
    ASSERT_TRUE("run command exists", cmd != NULL);
    if (!cmd) return;

    g_cmd_args_calls = 0;
    hide_window_calls = 0;
    g_cmd_args_result = COFI_HANDLED_HIDE;
    gboolean result = cmd->handler(&app, NULL, "xterm");
    ASSERT_TRUE("run with arg returns FALSE", result == FALSE);
    ASSERT_TRUE("run with arg dispatches to provider", g_cmd_args_calls == 1);
    ASSERT_TRUE("run with arg passes command string", strcmp(g_cmd_args_last, "xterm") == 0);
    ASSERT_TRUE("run with arg hides after provider hide status", hide_window_calls == 1);

    g_cmd_args_calls = 0;
    g_enter_modal_calls_cmd = 0;
    hide_window_calls = 0;
    result = cmd->handler(&app, NULL, "");
    ASSERT_TRUE("run without arg returns FALSE", result == FALSE);
    ASSERT_TRUE("run without arg does not dispatch args", g_cmd_args_calls == 0);
    ASSERT_TRUE("run without arg enters run modal", g_enter_modal_calls_cmd == 1);
    ASSERT_TRUE("run without arg does not hide", hide_window_calls == 0);
}

static void test_cmd_calc_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    memset(&g_stub_calc_provider, 0, sizeof(g_stub_calc_provider));
    g_stub_calc_provider.tab_mode = TAB_CALC;

    const CommandDef *cmd = find_command("calc");
    ASSERT_TRUE("calc command exists", cmd != NULL);
    if (!cmd) return;

    g_cmd_args_calls = 0;
    g_enter_modal_calls_cmd = 0;
    g_cmd_args_result = COFI_HANDLED_KEEP;
    gboolean result = cmd->handler(&app, NULL, "1+1");
    ASSERT_TRUE("calc with arg returns FALSE", result == FALSE);
    ASSERT_TRUE("calc with arg enters calc modal", g_enter_modal_calls_cmd == 1);
    ASSERT_TRUE("calc with arg dispatches to provider", g_cmd_args_calls == 1);
    ASSERT_TRUE("calc with arg passes expression", strcmp(g_cmd_args_last, "1+1") == 0);

    g_cmd_args_calls = 0;
    g_enter_modal_calls_cmd = 0;
    result = cmd->handler(&app, NULL, "");
    ASSERT_TRUE("calc without arg returns FALSE", result == FALSE);
    ASSERT_TRUE("calc without arg enters calc modal", g_enter_modal_calls_cmd == 1);
    ASSERT_TRUE("calc without arg does not dispatch args", g_cmd_args_calls == 0);
}

static void test_cmd_profiles_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    memset(&g_stub_profiles_provider, 0, sizeof(g_stub_profiles_provider));
    g_stub_profiles_provider.tab_mode = TAB_PROFILES;

    const CommandDef *cmd = find_command("profiles");
    ASSERT_TRUE("profiles command exists", cmd != NULL);
    if (!cmd) return;

    g_cmd_args_calls = 0;
    hide_window_calls = 0;
    g_cmd_args_result = COFI_HANDLED_HIDE;
    gboolean result = cmd->handler(&app, NULL, "gs");
    ASSERT_TRUE("profiles with arg returns FALSE", result == FALSE);
    ASSERT_TRUE("profiles with arg dispatches to provider", g_cmd_args_calls == 1);
    ASSERT_TRUE("profiles with arg passes query", strcmp(g_cmd_args_last, "gs") == 0);
    ASSERT_TRUE("profiles with arg hides after provider hide status", hide_window_calls == 1);

    g_cmd_args_calls = 0;
    hide_window_calls = 0;
    app.current_tab = TAB_WINDOWS;
    result = cmd->handler(&app, NULL, "");
    ASSERT_TRUE("profiles without arg returns FALSE", result == FALSE);
    ASSERT_TRUE("profiles without arg does not dispatch args", g_cmd_args_calls == 0);
    ASSERT_TRUE("profiles without arg surfaces tab", app.current_tab == TAB_PROFILES);
    ASSERT_TRUE("profiles without arg does not hide", hide_window_calls == 0);
}

int main(void) {
    printf("Command handler behavior regression tests\n");
    printf("========================================\n\n");

    test_window_handler_behavior();
    test_window_state_handlers_behavior();
    test_workspace_handler_behavior();
    test_jump_slot_handler_behavior();
    test_tiling_handler_behavior();
    test_ui_handler_behavior();
    test_cmd_run_behavior();
    test_cmd_calc_behavior();
    test_cmd_profiles_behavior();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
