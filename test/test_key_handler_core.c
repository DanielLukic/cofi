#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../src/app_data.h"
#include "../src/key_handler.h"
#include "../src/constants.h"

/*
 * Testability strategy:
 * Include key_handler.c directly and stub external deps.
 * Stubs capture arguments + call routing to assert behavior (not structure).
 */

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

/* --- capture state --- */
static int g_hide_calls;
static int g_repeat_calls;
static int g_activate_calls;
static Window g_last_activate_window;
static Display *g_last_activate_display;
static int g_switch_calls;
static int g_last_switched_desktop;
static int g_apps_launch_calls;
static const AppEntry *g_last_app_entry;
static int g_workspace_switch_state;
static int g_highlight_calls;
static Window g_last_highlight_window;
static char g_last_windows_query[256];

static gboolean g_overlay_active;
static int g_handle_overlay_calls;
static gboolean g_overlay_handler_returns;

static int g_handle_command_calls;
static gboolean g_command_handler_returns;

static int g_handle_run_calls;
static gboolean g_run_handler_returns;

static int g_handle_tab_switching_calls;
static gboolean g_tab_switching_returns;

static int g_enter_command_mode_calls;
static int g_enter_run_mode_calls;

static int g_move_selection_up_calls;
static int g_move_selection_down_calls;

static int g_filter_windows_calls;
static int g_filter_workspaces_calls;
static int g_filter_harpoon_calls;
static int g_filter_names_calls;
static int g_filter_config_calls;
static int g_filter_hotkeys_calls;
static int g_filter_apps_calls;

static char g_last_filter_windows[64];
static char g_last_filter_workspaces[64];
static char g_last_filter_harpoon[64];
static char g_last_filter_names[64];
static char g_last_filter_config[64];
static char g_last_filter_hotkeys[64];
static char g_last_filter_apps[64];

static int g_reset_selection_calls;
static int g_update_display_calls;
static int g_run_entry_changed_calls;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

/* --- Stubs required by key_handler.c --- */
gboolean is_overlay_active(AppData *app) { (void)app; return g_overlay_active; }

gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event) {
    (void)app;
    (void)event;
    g_handle_overlay_calls++;
    return g_overlay_handler_returns;
}

gboolean handle_command_key(GdkEventKey *event, AppData *app) {
    (void)event;
    (void)app;
    g_handle_command_calls++;
    return g_command_handler_returns;
}

void command_update_candidates(CommandMode *cmd, const char *text) {
    (void)cmd;
    (void)text;
}

gboolean handle_run_key(GdkEventKey *event, AppData *app) {
    (void)event;
    (void)app;
    g_handle_run_calls++;
    return g_run_handler_returns;
}

gboolean handle_tab_switching(GdkEventKey *event, AppData *app) {
    (void)event;
    (void)app;
    g_handle_tab_switching_calls++;
    return g_tab_switching_returns;
}

void enter_command_mode(AppData *app) {
    g_enter_command_mode_calls++;
    app->command_mode.state = CMD_MODE_COMMAND;
    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ":");
    }
}

void enter_run_mode(AppData *app, const char *prefill_command) {
    g_enter_run_mode_calls++;
    app->command_mode.state = CMD_MODE_RUN;
    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), "!");
    }
    if (app->entry) {
        gtk_entry_set_text(GTK_ENTRY(app->entry), prefill_command ? prefill_command : "");
    }
}

void handle_run_entry_changed(GtkEntry *entry, AppData *app) {
    (void)entry;
    (void)app;
    g_run_entry_changed_calls++;
}

WindowInfo *get_selected_window(AppData *app) {
    if (!app || app->current_tab != TAB_WINDOWS || app->filtered_count <= 0) return NULL;
    if (app->selection.window_index < 0 || app->selection.window_index >= app->filtered_count) return NULL;
    return &app->filtered[app->selection.window_index];
}

WorkspaceInfo *get_selected_workspace(AppData *app) {
    if (!app || app->current_tab != TAB_WORKSPACES || app->filtered_workspace_count <= 0) return NULL;
    if (app->selection.workspace_index < 0 || app->selection.workspace_index >= app->filtered_workspace_count) return NULL;
    return &app->filtered_workspaces[app->selection.workspace_index];
}

void move_selection_up(AppData *app) {
    g_move_selection_up_calls++;
    if (!app) return;
    if (app->current_tab == TAB_WINDOWS && app->filtered_count > 0) {
        if (app->selection.window_index < app->filtered_count - 1) {
            app->selection.window_index++;
        } else {
            app->selection.window_index = 0;
        }
    }
}

void move_selection_down(AppData *app) {
    g_move_selection_down_calls++;
    if (!app) return;
    if (app->current_tab == TAB_WINDOWS && app->filtered_count > 0) {
        if (app->selection.window_index > 0) {
            app->selection.window_index--;
        } else {
            app->selection.window_index = app->filtered_count - 1;
        }
    }
}

void store_last_windows_query(AppData *app, const char *query) {
    (void)app;
    strncpy(g_last_windows_query, query ? query : "", sizeof(g_last_windows_query) - 1);
    g_last_windows_query[sizeof(g_last_windows_query) - 1] = '\0';
}

void handle_repeat_key(AppData *app) { (void)app; g_repeat_calls++; }
void set_workspace_switch_state(int state) { g_workspace_switch_state = state; }

void activate_window(Display *display, Window window_id) {
    g_activate_calls++;
    g_last_activate_display = display;
    g_last_activate_window = window_id;
}

void highlight_window(AppData *app, Window window_id) {
    (void)app;
    g_highlight_calls++;
    g_last_highlight_window = window_id;
}

void hide_window(AppData *app) {
    g_hide_calls++;
    app->window_visible = FALSE;
}

void switch_to_desktop(Display *display, int desktop) {
    (void)display;
    g_switch_calls++;
    g_last_switched_desktop = desktop;
}

void apps_launch(const AppEntry *entry) {
    g_apps_launch_calls++;
    g_last_app_entry = entry;
}

/* required but not exercised here */
int get_number_of_desktops(Display *display) { (void)display; return 0; }
void assign_workspace_slots(AppData *app) { (void)app; }
Window get_workspace_slot_window(const WorkspaceSlotManager *manager, int slot) { (void)manager; (void)slot; return 0; }
Window get_slot_window(const HarpoonManager *manager, int slot) { (void)manager; (void)slot; return 0; }
int get_window_slot(const HarpoonManager *manager, Window id) { (void)manager; (void)id; return -1; }
void unassign_slot(HarpoonManager *manager, int slot) { (void)manager; (void)slot; }
void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window) { (void)manager; (void)slot; (void)window; }
void save_harpoon_slots(const HarpoonManager *manager) { (void)manager; }
void save_config(const CofiConfig *config) { (void)config; }
void show_name_edit_overlay(AppData *app) { (void)app; }
void show_name_delete_overlay(AppData *app, const char *custom_name, int manager_index) {
    (void)app; (void)custom_name; (void)manager_index;
}
int find_named_window_index(const NamedWindowManager *manager, Window id) { (void)manager; (void)id; return -1; }
int find_named_window_by_name(const NamedWindowManager *manager, const char *custom_name) { (void)manager; (void)custom_name; return -1; }
void delete_custom_name(NamedWindowManager *manager, int index) { (void)manager; (void)index; }
void save_named_windows(const NamedWindowManager *manager) { (void)manager; }
void show_harpoon_delete_overlay(AppData *app, int slot) { (void)app; (void)slot; }
void show_harpoon_edit_overlay(AppData *app, int slot) { (void)app; (void)slot; }
const char *get_next_enum_value(const char *key, const char *current_value) { (void)key; (void)current_value; return NULL; }
int apply_config_setting(CofiConfig *config, const char *key, const char *value, char *err_buf, size_t err_size) {
    (void)config; (void)key; (void)value; (void)err_buf; (void)err_size; return 0;
}
void show_overlay(AppData *app, OverlayType type, void *data) { (void)app; (void)type; (void)data; }
void cleanup_hotkeys(AppData *app) { (void)app; }
int remove_hotkey_binding(HotkeyConfig *config, const char *key) { (void)config; (void)key; return 0; }
int save_hotkey_config(const HotkeyConfig *config) { (void)config; return 0; }
void regrab_hotkeys(AppData *app) { (void)app; }

void filter_windows(AppData *app, const char *query) {
    (void)app;
    g_filter_windows_calls++;
    strncpy(g_last_filter_windows, query ? query : "", sizeof(g_last_filter_windows) - 1);
}
void filter_workspaces(AppData *app, const char *query) {
    (void)app;
    g_filter_workspaces_calls++;
    strncpy(g_last_filter_workspaces, query ? query : "", sizeof(g_last_filter_workspaces) - 1);
}
void filter_harpoon(AppData *app, const char *filter) {
    (void)app;
    g_filter_harpoon_calls++;
    strncpy(g_last_filter_harpoon, filter ? filter : "", sizeof(g_last_filter_harpoon) - 1);
}
void filter_names(AppData *app, const char *filter) {
    (void)app;
    g_filter_names_calls++;
    strncpy(g_last_filter_names, filter ? filter : "", sizeof(g_last_filter_names) - 1);
}
void filter_config(AppData *app, const char *filter) {
    (void)app;
    g_filter_config_calls++;
    strncpy(g_last_filter_config, filter ? filter : "", sizeof(g_last_filter_config) - 1);
}
void filter_hotkeys(AppData *app, const char *filter) {
    (void)app;
    g_filter_hotkeys_calls++;
    strncpy(g_last_filter_hotkeys, filter ? filter : "", sizeof(g_last_filter_hotkeys) - 1);
}
void filter_apps(AppData *app, const char *query) {
    (void)app;
    g_filter_apps_calls++;
    strncpy(g_last_filter_apps, query ? query : "", sizeof(g_last_filter_apps) - 1);
}

void reset_selection(AppData *app) { (void)app; g_reset_selection_calls++; }
void update_display(AppData *app) { (void)app; g_update_display_calls++; }

#include "../src/key_handler.c"

static void reset_captures(void) {
    g_hide_calls = 0;
    g_repeat_calls = 0;
    g_activate_calls = 0;
    g_last_activate_window = 0;
    g_last_activate_display = NULL;
    g_switch_calls = 0;
    g_last_switched_desktop = -1;
    g_apps_launch_calls = 0;
    g_last_app_entry = NULL;
    g_workspace_switch_state = 0;
    g_highlight_calls = 0;
    g_last_highlight_window = 0;
    g_last_windows_query[0] = '\0';

    g_overlay_active = FALSE;
    g_handle_overlay_calls = 0;
    g_overlay_handler_returns = FALSE;
    g_handle_command_calls = 0;
    g_command_handler_returns = FALSE;
    g_handle_run_calls = 0;
    g_run_handler_returns = FALSE;
    g_handle_tab_switching_calls = 0;
    g_tab_switching_returns = FALSE;
    g_enter_command_mode_calls = 0;
    g_enter_run_mode_calls = 0;
    g_move_selection_up_calls = 0;
    g_move_selection_down_calls = 0;

    g_filter_windows_calls = 0;
    g_filter_workspaces_calls = 0;
    g_filter_harpoon_calls = 0;
    g_filter_names_calls = 0;
    g_filter_config_calls = 0;
    g_filter_hotkeys_calls = 0;
    g_filter_apps_calls = 0;
    g_last_filter_windows[0] = '\0';
    g_last_filter_workspaces[0] = '\0';
    g_last_filter_harpoon[0] = '\0';
    g_last_filter_names[0] = '\0';
    g_last_filter_config[0] = '\0';
    g_last_filter_hotkeys[0] = '\0';
    g_last_filter_apps[0] = '\0';
    g_reset_selection_calls = 0;
    g_update_display_calls = 0;
    g_run_entry_changed_calls = 0;
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->entry = gtk_entry_new();
    app->mode_indicator = gtk_label_new(">");
    app->window_visible = TRUE;
    app->command_mode.state = CMD_MODE_NORMAL;
}

static GdkEventKey make_key(guint keyval, GdkModifierType state) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = keyval;
    event.state = state;
    return event;
}

static void test_escape_windows_hides(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TAB_WINDOWS;

    GdkEventKey ev = make_key(GDK_KEY_Escape, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Escape on Windows tab handled", handled == TRUE);
    ASSERT_TRUE("Escape on Windows tab hides window", app.window_visible == FALSE && g_hide_calls == 1);
}

static void test_escape_harpoon_pending_delete_cancels_only(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TAB_HARPOON;
    app.harpoon_delete.pending_delete = TRUE;

    GdkEventKey ev = make_key(GDK_KEY_Escape, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Escape on Harpoon pending-delete handled", handled == TRUE);
    ASSERT_TRUE("Escape on Harpoon clears pending_delete", app.harpoon_delete.pending_delete == FALSE);
    ASSERT_TRUE("Escape on Harpoon pending-delete does not hide", g_hide_calls == 0 && app.window_visible == TRUE);
}

static void test_return_windows_activates_selected_and_hides(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 2;
    app.selection.window_index = 1;
    app.filtered[1].id = (Window)0x222;
    app.display = (Display *)0xABC;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "term");

    GdkEventKey ev = make_key(GDK_KEY_Return, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Return on Windows handled", handled == TRUE);
    ASSERT_TRUE("Return on Windows activates selected window",
                g_activate_calls == 1 && g_last_activate_window == (Window)0x222 && g_last_activate_display == (Display *)0xABC);
    ASSERT_TRUE("Return on Windows records last query", strcmp(g_last_windows_query, "term") == 0);
    ASSERT_TRUE("Return on Windows sets workspace switch state", g_workspace_switch_state == 1);
    ASSERT_TRUE("Return on Windows highlights activated window",
                g_highlight_calls == 1 && g_last_highlight_window == (Window)0x222);
    ASSERT_TRUE("Return on Windows hides window", g_hide_calls == 1 && app.window_visible == FALSE);
}

static void test_return_apps_launches_selected_and_hides(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_APPS;
    app.filtered_apps_count = 1;
    app.selection.apps_index = 0;
    strcpy(app.filtered_apps[0].name, "Firefox");

    GdkEventKey ev = make_key(GDK_KEY_Return, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Return on Apps handled", handled == TRUE);
    ASSERT_TRUE("Return on Apps launches selected app",
                g_apps_launch_calls == 1 && g_last_app_entry == &app.filtered_apps[0]);
    ASSERT_TRUE("Return on Apps hides window", g_hide_calls == 1 && app.window_visible == FALSE);
}

static void test_return_workspaces_switches_desktop_and_hides(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WORKSPACES;
    app.filtered_workspace_count = 1;
    app.selection.workspace_index = 0;
    app.filtered_workspaces[0].id = 3;
    strcpy(app.filtered_workspaces[0].name, "WS4");

    GdkEventKey ev = make_key(GDK_KEY_Return, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Return on Workspaces handled", handled == TRUE);
    ASSERT_TRUE("Return on Workspaces switches selected workspace",
                g_switch_calls == 1 && g_last_switched_desktop == 3);
    ASSERT_TRUE("Return on Workspaces hides window", g_hide_calls == 1 && app.window_visible == FALSE);
}

static void test_up_arrow_moves_selection(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 3;
    app.selection.window_index = 0;

    GdkEventKey ev = make_key(GDK_KEY_Up, 0);
    on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Up arrow moves selection", app.selection.window_index == 1);
}

static void test_down_arrow_moves_selection(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 3;
    app.selection.window_index = 1;

    GdkEventKey ev = make_key(GDK_KEY_Down, 0);
    on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Down arrow moves selection down", app.selection.window_index == 0);
}

static void test_ctrl_k_matches_up_behavior(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 3;
    app.selection.window_index = 0;

    GdkEventKey ev = make_key(GDK_KEY_k, GDK_CONTROL_MASK);
    on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+k moves selection like Up", app.selection.window_index == 1);
}

static void test_ctrl_j_matches_down_behavior(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 3;
    app.selection.window_index = 2;

    GdkEventKey ev = make_key(GDK_KEY_j, GDK_CONTROL_MASK);
    on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+j moves selection like Down", app.selection.window_index == 1);
}

static void test_alt_tab_moves_selection_up_on_windows_tab(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 3;
    app.selection.window_index = 0;

    GdkEventKey ev = make_key(GDK_KEY_Tab, GDK_MOD1_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Alt+Tab on Windows handled", handled == TRUE);
    ASSERT_TRUE("Alt+Tab on Windows moves selection up", app.selection.window_index == 1 && g_move_selection_up_calls == 1);
}

static void test_alt_shift_tab_moves_selection_down_on_windows_tab(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 3;
    app.selection.window_index = 1;

    GdkEventKey ev = make_key(GDK_KEY_ISO_Left_Tab, GDK_MOD1_MASK | GDK_SHIFT_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Alt+Shift+Tab on Windows handled", handled == TRUE);
    ASSERT_TRUE("Alt+Shift+Tab on Windows moves selection down", app.selection.window_index == 0 && g_move_selection_down_calls == 1);
}

static void test_period_empty_query_calls_repeat(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");

    GdkEventKey ev = make_key(GDK_KEY_period, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Period empty query handled", handled == TRUE);
    ASSERT_TRUE("Period empty query calls repeat handler", g_repeat_calls == 1);
}

static void test_colon_enters_command_mode(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.command_mode.state = CMD_MODE_NORMAL;

    GdkEventKey ev = make_key(GDK_KEY_colon, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Colon handled", handled == TRUE);
    ASSERT_TRUE("Colon enters command mode", app.command_mode.state == CMD_MODE_COMMAND && g_enter_command_mode_calls == 1);
    ASSERT_TRUE("Colon sets mode indicator ':'",
                strcmp(gtk_label_get_text(GTK_LABEL(app.mode_indicator)), ":") == 0);
}

static void test_exclam_enters_run_mode_with_empty_entry(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.command_mode.state = CMD_MODE_NORMAL;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "leftover");

    GdkEventKey ev = make_key(GDK_KEY_exclam, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Exclam handled", handled == TRUE);
    ASSERT_TRUE("Exclam enters run mode", app.command_mode.state == CMD_MODE_RUN && g_enter_run_mode_calls == 1);
    ASSERT_TRUE("Exclam sets mode indicator '!'",
                strcmp(gtk_label_get_text(GTK_LABEL(app.mode_indicator)), "!") == 0);
    ASSERT_TRUE("Exclam run-mode entry is empty", strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "") == 0);
}

static void test_overlay_dispatch_precedence(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WINDOWS;
    app.filtered_count = 1;
    app.filtered[0].id = (Window)0x101;
    g_overlay_active = TRUE;
    g_overlay_handler_returns = TRUE;

    GdkEventKey ev = make_key(GDK_KEY_Return, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Overlay active routes to overlay handler", handled == TRUE && g_handle_overlay_calls == 1);
    ASSERT_TRUE("Overlay active does not hit main dispatch", g_activate_calls == 0 && g_hide_calls == 0);
    ASSERT_TRUE("Overlay active bypasses command/run handlers", g_handle_command_calls == 0 && g_handle_run_calls == 0);
}

static void test_command_mode_dispatch_precedence(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.command_mode.state = CMD_MODE_COMMAND;
    g_command_handler_returns = TRUE;

    GdkEventKey ev = make_key(GDK_KEY_colon, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("CMD_MODE_COMMAND routes to command handler", handled == TRUE && g_handle_command_calls == 1);
    ASSERT_TRUE("CMD_MODE_COMMAND does not hit normal dispatch", g_enter_command_mode_calls == 0 && g_activate_calls == 0);
}

static void test_run_mode_dispatch_precedence(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.command_mode.state = CMD_MODE_RUN;
    g_run_handler_returns = TRUE;

    GdkEventKey ev = make_key(GDK_KEY_Return, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("CMD_MODE_RUN routes to run handler", handled == TRUE && g_handle_run_calls == 1);
    ASSERT_TRUE("CMD_MODE_RUN does not hit navigation dispatch", g_activate_calls == 0 && g_hide_calls == 0);
}

static void test_on_entry_changed_routes_per_tab_filters(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    TabMode tabs[] = {
        TAB_WINDOWS, TAB_WORKSPACES, TAB_HARPOON,
        TAB_NAMES, TAB_CONFIG, TAB_HOTKEYS, TAB_APPS
    };

    for (int i = 0; i < 7; i++) {
        g_filter_windows_calls = g_filter_workspaces_calls = g_filter_harpoon_calls = 0;
        g_filter_names_calls = g_filter_config_calls = g_filter_hotkeys_calls = 0;
        g_filter_apps_calls = 0;
        g_reset_selection_calls = 0;
        g_update_display_calls = 0;

        app.current_tab = tabs[i];
        gtk_entry_set_text(GTK_ENTRY(app.entry), "query");
        on_entry_changed(GTK_ENTRY(app.entry), &app);

        int sum = g_filter_windows_calls + g_filter_workspaces_calls + g_filter_harpoon_calls +
                  g_filter_names_calls + g_filter_config_calls + g_filter_hotkeys_calls + g_filter_apps_calls;

        ASSERT_TRUE("on_entry_changed calls exactly one tab filter", sum == 1);
        ASSERT_TRUE("on_entry_changed calls reset_selection", g_reset_selection_calls == 1);
        ASSERT_TRUE("on_entry_changed calls update_display", g_update_display_calls == 1);

        ASSERT_TRUE("WINDOWS filter routing",
                    tabs[i] != TAB_WINDOWS || (g_filter_windows_calls == 1 && strcmp(g_last_filter_windows, "query") == 0));
        ASSERT_TRUE("WORKSPACES filter routing",
                    tabs[i] != TAB_WORKSPACES || (g_filter_workspaces_calls == 1 && strcmp(g_last_filter_workspaces, "query") == 0));
        ASSERT_TRUE("HARPOON filter routing",
                    tabs[i] != TAB_HARPOON || (g_filter_harpoon_calls == 1 && strcmp(g_last_filter_harpoon, "query") == 0));
        ASSERT_TRUE("NAMES filter routing",
                    tabs[i] != TAB_NAMES || (g_filter_names_calls == 1 && strcmp(g_last_filter_names, "query") == 0));
        ASSERT_TRUE("CONFIG filter routing",
                    tabs[i] != TAB_CONFIG || (g_filter_config_calls == 1 && strcmp(g_last_filter_config, "query") == 0));
        ASSERT_TRUE("HOTKEYS filter routing",
                    tabs[i] != TAB_HOTKEYS || (g_filter_hotkeys_calls == 1 && strcmp(g_last_filter_hotkeys, "query") == 0));
        ASSERT_TRUE("APPS filter routing",
                    tabs[i] != TAB_APPS || (g_filter_apps_calls == 1 && strcmp(g_last_filter_apps, "query") == 0));
    }
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Key handler core tests\n");
        printf("======================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Key handler core tests\n");
    printf("======================\n\n");

    test_escape_windows_hides();
    test_escape_harpoon_pending_delete_cancels_only();
    test_return_windows_activates_selected_and_hides();
    test_return_apps_launches_selected_and_hides();
    test_return_workspaces_switches_desktop_and_hides();
    test_up_arrow_moves_selection();
    test_down_arrow_moves_selection();
    test_ctrl_k_matches_up_behavior();
    test_ctrl_j_matches_down_behavior();
    test_alt_tab_moves_selection_up_on_windows_tab();
    test_alt_shift_tab_moves_selection_down_on_windows_tab();
    test_period_empty_query_calls_repeat();
    test_colon_enters_command_mode();
    test_exclam_enters_run_mode_with_empty_entry();
    test_overlay_dispatch_precedence();
    test_command_mode_dispatch_precedence();
    test_run_mode_dispatch_precedence();
    test_on_entry_changed_routes_per_tab_filters();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
