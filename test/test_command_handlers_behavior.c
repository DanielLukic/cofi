#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/command_registry.h"
#include "../src/core_commands.h"
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
static int g_save_layout_calls = 0;
static int g_restore_layout_calls = 0;
static int g_clear_layout_calls = 0;
static int g_save_match_entries_calls = 0;
static int g_save_harpoon_slots_calls = 0;
static Window g_harpoon_slots[MAX_HARPOON_SLOTS] = {0};

// --- shared stubs for handler dependencies ---
void xmove_resize_frame_aware(Display *display, Window window,
                               int frame_x, int frame_y, int width, int height) {
    (void)display; (void)window; (void)frame_x; (void)frame_y; (void)width; (void)height;
}
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
static CofiTabProvider g_stub_config_provider;
static CofiTabProvider g_stub_hotkeys_provider;
static CofiTabProvider g_stub_rules_provider;
static int g_cmd_args_calls = 0;
static char g_cmd_args_last[256] = {0};
static CofiActionStatus g_cmd_args_result = COFI_HANDLED_HIDE;

#define TEST_CALC_TAB ((TabMode)(TAB_COUNT + 1))
#define TEST_RUN_TAB ((TabMode)(TAB_COUNT + 2))
#define TEST_CONFIG_TAB ((TabMode)(TAB_COUNT + 3))
#define TEST_HOTKEYS_TAB ((TabMode)(TAB_COUNT + 4))
#define TEST_RULES_TAB ((TabMode)(TAB_COUNT + 5))

static gboolean noop_provider_command(AppData *app, WindowInfo *window, const char *args) {
    (void)app;
    (void)window;
    (void)args;
    return TRUE;
}

static const CommandSpec s_rules_command = {
    .primary = "rules",
    .aliases = {"rs", NULL},
    .owner_provider_id = "rules",
    .handler = noop_provider_command,
};

static void init_stub_providers(void) {
    cofi_command_registry_reset();
    cofi_register_core_commands();
    cofi_register_command(&s_rules_command);

    memset(&g_stub_run_provider, 0, sizeof(g_stub_run_provider));
    memset(&g_stub_calc_provider, 0, sizeof(g_stub_calc_provider));
    memset(&g_stub_config_provider, 0, sizeof(g_stub_config_provider));
    memset(&g_stub_hotkeys_provider, 0, sizeof(g_stub_hotkeys_provider));
    memset(&g_stub_rules_provider, 0, sizeof(g_stub_rules_provider));

    g_stub_run_provider.tab_mode = TEST_RUN_TAB;
    g_stub_calc_provider.tab_mode = TEST_CALC_TAB;
    g_stub_config_provider.tab_mode = TEST_CONFIG_TAB;
    g_stub_hotkeys_provider.tab_mode = TEST_HOTKEYS_TAB;
    g_stub_rules_provider.tab_mode = TEST_RULES_TAB;
}

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
TabMode config_tab_mode(void) { return TEST_CONFIG_TAB; }
TabMode hotkeys_tab_mode(void) { return TEST_HOTKEYS_TAB; }
int cofi_get_provider_id(const char *id) {
    if (id && strcmp(id, "calc") == 0) return 1;
    if (id && strcmp(id, "config") == 0) return 2;
    if (id && strcmp(id, "hotkeys") == 0) return 3;
    if (id && strcmp(id, "rules") == 0) return 4;
    return -1;
}
int cofi_provider_is_enabled(int provider_id) {
    return provider_id > 0;
}
int cofi_provider_count(void) { return 0; }
const CofiTabProvider *cofi_get_provider(int provider_id) {
    if (provider_id == 1) return &g_stub_calc_provider;
    if (provider_id == 2) return &g_stub_config_provider;
    if (provider_id == 3) return &g_stub_hotkeys_provider;
    if (provider_id == 4) return &g_stub_rules_provider;
    return NULL;
}
const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    (void)tab_mode;
    return &g_stub_calc_provider;
}
int cofi_get_provider_id_for_tab(int tab_mode) { (void)tab_mode; return -1; }
void cofi_apply_disabled_providers(const char *disabled_ids) { (void)disabled_ids; }
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
void set_window_name(Display *display, Window window, const char *name) {
    (void)display;
    (void)window;
    (void)name;
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
gboolean save_window_geometry_for_window(AppData *app, const WindowInfo *window) {
    (void)app;
    (void)window;
    g_save_layout_calls++;
    return TRUE;
}
gboolean restore_window_geometry_for_window(AppData *app, const WindowInfo *window) {
    (void)app;
    (void)window;
    g_restore_layout_calls++;
    return TRUE;
}
gboolean clear_window_geometry_for_window(AppData *app, const WindowInfo *window) {
    (void)app;
    (void)window;
    g_clear_layout_calls++;
    return TRUE;
}
void save_match_entries(const MatchEntryManager *manager) {
    (void)manager;
    g_save_match_entries_calls++;
}
void save_harpoon_slots(const HarpoonManager *manager) {
    (void)manager;
    g_save_harpoon_slots_calls++;
}
gboolean harpoon_assign_or_toggle_window(AppData *app, WindowInfo *selected_window, int slot) {
    if (!app || !selected_window || slot < 0 || slot >= MAX_HARPOON_SLOTS) {
        return FALSE;
    }

    if (g_harpoon_slots[slot] == selected_window->id) {
        g_harpoon_slots[slot] = 0;
    } else {
        for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
            if (g_harpoon_slots[i] == selected_window->id) {
                g_harpoon_slots[i] = 0;
            }
        }
        g_harpoon_slots[slot] = selected_window->id;
    }

    save_match_entries(&app->matching);
    save_harpoon_slots(&app->harpoon);
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

static void test_window_handler_behavior(void) {
    AppData app;
    WindowInfo window;
    memset(&app, 0, sizeof(app));
    memset(&window, 0, sizeof(window));

    const CommandSpec *cmd = cofi_command_by_primary("an");
    ASSERT_TRUE("window command 'an' exists", cmd != NULL);

    if (!cmd) return;

    app.current_tab = TEST_HOTKEYS_TAB;
    gboolean no_window_result = cmd->handler(&app, NULL, "");
    ASSERT_TRUE("an returns TRUE on missing window", no_window_result == TRUE);

    gboolean wrong_tab_result = cmd->handler(&app, &window, "");
    ASSERT_TRUE("an returns TRUE outside windows tab", wrong_tab_result == TRUE);

    app.current_tab = TAB_WINDOWS;
    show_name_assign_overlay_calls = 0;
    gboolean open_overlay_result = cmd->handler(&app, &window, "");
    ASSERT_TRUE("an returns FALSE when opening overlay", open_overlay_result == FALSE);
    ASSERT_TRUE("an opens name assign overlay", show_name_assign_overlay_calls == 1);

    memset(&window, 0, sizeof(window));
    window.id = 0xCAFE;
    g_strlcpy(window.title, "Terminal", sizeof(window.title));
    g_strlcpy(window.class_name, "Alacritty", sizeof(window.class_name));
    g_strlcpy(window.instance, "term", sizeof(window.instance));

    show_name_assign_overlay_calls = 0;
    hide_window_calls = 0;
    g_save_match_entries_calls = 0;
    gboolean inline_result = cmd->handler(&app, &window, "mywin");
    int inline_idx = match_entry_find_index_by_window(&app.matching, window.id);

    ASSERT_TRUE("an inline returns TRUE", inline_result == TRUE);
    ASSERT_TRUE("an inline persists entries", g_save_match_entries_calls == 1);
    ASSERT_TRUE("an inline hides window", hide_window_calls == 1);
    ASSERT_TRUE("an inline does not open overlay", show_name_assign_overlay_calls == 0);
    ASSERT_TRUE("an inline creates/updates match entry", inline_idx >= 0);
    ASSERT_TRUE("an inline sets custom name",
                inline_idx >= 0 && strcmp(app.matching.entries[inline_idx].custom_name, "mywin") == 0);

    show_name_assign_overlay_calls = 0;
    hide_window_calls = 0;
    g_save_match_entries_calls = 0;
    gboolean whitespace_result = cmd->handler(&app, &window, "   ");

    ASSERT_TRUE("an whitespace returns FALSE (overlay path)", whitespace_result == FALSE);
    ASSERT_TRUE("an whitespace opens overlay", show_name_assign_overlay_calls == 1);
    ASSERT_TRUE("an whitespace does not persist entries", g_save_match_entries_calls == 0);
    ASSERT_TRUE("an whitespace does not hide window", hide_window_calls == 0);
}

static void test_harpoon_set_handler_behavior(void) {
    AppData app;
    WindowInfo window;
    memset(&app, 0, sizeof(app));
    memset(&window, 0, sizeof(window));
    window.id = 0xBEEF;

    const CommandSpec *cmd = cofi_command_by_primary("hs");
    ASSERT_TRUE("hs command exists", cmd != NULL);
    if (!cmd) return;
    ASSERT_TRUE("hs alias resolves", cofi_command_for_token("harpoon-set") == cmd);

    g_save_harpoon_slots_calls = 0;
    g_save_match_entries_calls = 0;
    hide_window_calls = 0;
    memset(g_harpoon_slots, 0, sizeof(g_harpoon_slots));

    app.current_tab = TEST_HOTKEYS_TAB;
    ASSERT_TRUE("hs rejects outside windows tab", cmd->handler(&app, &window, "1") == TRUE);
    ASSERT_TRUE("hs wrong-tab no mutation",
                g_save_harpoon_slots_calls == 0 && g_save_match_entries_calls == 0 && hide_window_calls == 0);

    app.current_tab = TAB_WINDOWS;
    ASSERT_TRUE("hs rejects missing arg", cmd->handler(&app, &window, "") == TRUE);
    ASSERT_TRUE("hs rejects whitespace arg", cmd->handler(&app, &window, "   ") == TRUE);
    ASSERT_TRUE("hs rejects multi-char", cmd->handler(&app, &window, "12") == TRUE);
    ASSERT_TRUE("hs invalid args no mutation",
                g_save_harpoon_slots_calls == 0 && g_save_match_entries_calls == 0 && hide_window_calls == 0);

    ASSERT_TRUE("hs accepts h key", cmd->handler(&app, &window, "h") == TRUE);
    ASSERT_TRUE("hs h-key assigned to slot", g_harpoon_slots[17] == window.id);
    ASSERT_TRUE("hs h-key persists match entries", g_save_match_entries_calls == 1);
    ASSERT_TRUE("hs h-key persists", g_save_harpoon_slots_calls == 1);
    ASSERT_TRUE("hs h-key hides window", hide_window_calls == 1);

    hide_window_calls = 0;
    ASSERT_TRUE("hs assigns digit key", cmd->handler(&app, &window, "1") == TRUE);
    ASSERT_TRUE("hs digit clears previous slot", g_harpoon_slots[17] == 0);
    ASSERT_TRUE("hs digit assigned", g_harpoon_slots[1] == window.id);
    ASSERT_TRUE("hs digit persists match entries second time", g_save_match_entries_calls == 2);
    ASSERT_TRUE("hs digit persists second time", g_save_harpoon_slots_calls == 2);
    ASSERT_TRUE("hs digit hides window", hide_window_calls == 1);

    hide_window_calls = 0;
    ASSERT_TRUE("hs assigns letter key", cmd->handler(&app, &window, "a") == TRUE);
    ASSERT_TRUE("hs letter assigned", g_harpoon_slots[10] == window.id);
    ASSERT_TRUE("hs letter clears previous digit slot", g_harpoon_slots[1] == 0);
    ASSERT_TRUE("hs letter persists match entries third time", g_save_match_entries_calls == 3);
    ASSERT_TRUE("hs letter persists third time", g_save_harpoon_slots_calls == 3);
    ASSERT_TRUE("hs letter hides window", hide_window_calls == 1);

    hide_window_calls = 0;
    ASSERT_TRUE("hs assigns uppercase key", cmd->handler(&app, &window, "A") == TRUE);
    ASSERT_TRUE("hs uppercase toggles slot off", g_harpoon_slots[10] == 0);
    ASSERT_TRUE("hs uppercase persists match entries fourth time", g_save_match_entries_calls == 4);
    ASSERT_TRUE("hs uppercase persists fourth time", g_save_harpoon_slots_calls == 4);
    ASSERT_TRUE("hs uppercase hides window", hide_window_calls == 1);

    hide_window_calls = 0;
    g_save_match_entries_calls = 0;
    g_save_harpoon_slots_calls = 0;
    memset(g_harpoon_slots, 0, sizeof(g_harpoon_slots));
    ASSERT_TRUE("hs reassign same window slot1", cmd->handler(&app, &window, "1") == TRUE);
    ASSERT_TRUE("hs reassign same window slot2", cmd->handler(&app, &window, "2") == TRUE);
    ASSERT_TRUE("hs reassign clears old slot", g_harpoon_slots[1] == 0);
    ASSERT_TRUE("hs reassign stores new slot", g_harpoon_slots[2] == window.id);

    hide_window_calls = 0;
    g_save_match_entries_calls = 0;
    g_save_harpoon_slots_calls = 0;
    memset(g_harpoon_slots, 0, sizeof(g_harpoon_slots));
    WindowInfo new_window = window;
    new_window.id = 0xCAFE;
    ASSERT_TRUE("hs persists matching for new window", cmd->handler(&app, &new_window, "3") == TRUE);
    ASSERT_TRUE("hs new window save_match_entries called", g_save_match_entries_calls == 1);
    ASSERT_TRUE("hs new window save_harpoon_slots called", g_save_harpoon_slots_calls == 1);
}

static void test_window_state_handler(const char *cmd_name, const char *atom_name) {
    AppData app;
    WindowInfo window;
    memset(&app, 0, sizeof(app));
    memset(&window, 0, sizeof(window));
    window.id = 0xBEEF;

    const CommandSpec *cmd = cofi_command_by_primary(cmd_name);
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

static void test_layout_command_behavior(void) {
    AppData app;
    WindowInfo window;
    memset(&app, 0, sizeof(app));
    memset(&window, 0, sizeof(window));
    window.id = 0xBEEF;

    const CommandSpec *save_cmd = cofi_command_by_primary("save-layout");
    const CommandSpec *restore_cmd = cofi_command_by_primary("restore-layout");
    const CommandSpec *clear_cmd = cofi_command_by_primary("delete-layout");

    ASSERT_TRUE("save-layout command exists", save_cmd != NULL);
    ASSERT_TRUE("restore-layout command exists", restore_cmd != NULL);
    ASSERT_TRUE("delete-layout command exists", clear_cmd != NULL);
    if (!save_cmd || !restore_cmd || !clear_cmd) return;

    g_save_layout_calls = 0;
    ASSERT_TRUE("save-layout rejects missing window", save_cmd->handler(&app, NULL, "") == FALSE);
    ASSERT_TRUE("save-layout missing window is no-op", g_save_layout_calls == 0);
    ASSERT_TRUE("save-layout alias resolves", cofi_command_for_token("sl") == save_cmd);
    ASSERT_TRUE("save-layout dispatches helper", save_cmd->handler(&app, &window, "") == TRUE);
    ASSERT_TRUE("save-layout calls helper once", g_save_layout_calls == 1);

    g_restore_layout_calls = 0;
    ASSERT_TRUE("restore-layout rejects missing window", restore_cmd->handler(&app, NULL, "") == FALSE);
    ASSERT_TRUE("restore-layout missing window is no-op", g_restore_layout_calls == 0);
    ASSERT_TRUE("restore-layout alias resolves", cofi_command_for_token("rl") == restore_cmd);
    ASSERT_TRUE("restore-layout dispatches helper", restore_cmd->handler(&app, &window, "") == TRUE);
    ASSERT_TRUE("restore-layout calls helper once", g_restore_layout_calls == 1);

    g_clear_layout_calls = 0;
    ASSERT_TRUE("delete-layout rejects missing window", clear_cmd->handler(&app, NULL, "") == FALSE);
    ASSERT_TRUE("delete-layout missing window is no-op", g_clear_layout_calls == 0);
    ASSERT_TRUE("delete-layout alias resolves", cofi_command_for_token("dl") == clear_cmd);
    ASSERT_TRUE("delete-layout dispatches helper", clear_cmd->handler(&app, &window, "") == TRUE);
    ASSERT_TRUE("delete-layout calls helper once", g_clear_layout_calls == 1);
}

static void test_workspace_handler_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    const CommandSpec *cmd = cofi_command_by_primary("rw");
    ASSERT_TRUE("workspace command 'rw' exists", cmd != NULL);
    if (!cmd) return;

    gboolean invalid_workspace = cmd->handler(&app, NULL, "0");
    ASSERT_TRUE("rw rejects workspace 0", invalid_workspace == FALSE);
}

static void test_jump_slot_handler_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.display = (Display *)0x1;

    const CommandSpec *cmd = cofi_command_by_primary("jump-slot");
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

    const CommandSpec *cmd = cofi_command_by_primary("mouse");
    ASSERT_TRUE("tiling command 'mouse' exists", cmd != NULL);
    if (!cmd) return;

    gboolean result = cmd->handler(&app, NULL, "away");
    ASSERT_TRUE("mouse fails when display is unavailable", result == FALSE);
}

static void test_ui_handler_behavior(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    const CommandSpec *cmd = cofi_command_by_primary("show");
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
    ASSERT_TRUE("show rules surfaces rules tab", app.current_tab == TEST_RULES_TAB);

}

int main(void) {
    printf("Command handler behavior regression tests\n");
    printf("========================================\n\n");

    init_stub_providers();
    test_window_handler_behavior();
    test_harpoon_set_handler_behavior();
    test_window_state_handlers_behavior();
    test_layout_command_behavior();
    test_workspace_handler_behavior();
    test_jump_slot_handler_behavior();
    test_tiling_handler_behavior();
    test_ui_handler_behavior();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
