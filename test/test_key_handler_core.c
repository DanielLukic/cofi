#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
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
static int g_sinks_switch_calls;
static int g_proc_signal_calls;
static guint g_last_proc_signal_state;
static int g_provider_enter_calls;
static int g_provider_enter_modifier_state;
static int g_workspace_switch_state;
static int g_highlight_calls;
static Window g_last_highlight_window;
static char g_last_windows_query[256];

static gboolean g_overlay_active;
static int g_handle_overlay_calls;
static gboolean g_overlay_handler_returns;

static int g_handle_command_calls;
static gboolean g_command_handler_returns;
static int g_command_update_candidates_calls;
static char g_last_command_candidates_text[64];

static int g_handle_modal_calls;
static gboolean g_modal_handler_returns;

static int g_handle_tab_switching_calls;
static gboolean g_tab_switching_returns;

static int g_enter_command_mode_calls;
static int g_enter_modal_calls;
static int g_exit_modal_calls;

static int g_move_selection_up_calls;
static int g_move_selection_down_calls;

static int g_filter_windows_calls;
static int g_filter_workspaces_calls;
static int g_filter_harpoon_calls;
static int g_filter_names_calls;
static int g_filter_config_calls;
static int g_filter_hotkeys_calls;
static int g_filter_rules_calls;
static int g_filter_apps_calls;

static char g_last_filter_windows[64];
static char g_last_filter_workspaces[64];
static char g_last_filter_harpoon[64];
static char g_last_filter_names[64];
static char g_last_filter_config[64];
static char g_last_filter_hotkeys[64];
static char g_last_filter_rules[64];
static char g_last_filter_apps[64];

static int g_reset_selection_calls;
static int g_update_display_calls;
static int g_tab_prefix_lookup_calls;
static char g_last_tab_prefix_lookup;
static const CofiTabProvider *g_provider_for_tab;
static CofiTabProvider g_modal_prefix_stub;

#define TEST_WORKSPACES_TAB ((TabMode)(TAB_COUNT + 1))
#define TEST_HARPOON_TAB ((TabMode)(TAB_COUNT + 2))
#define TEST_MATCHING_TAB ((TabMode)(TAB_COUNT + 3))
#define TEST_CONFIG_TAB ((TabMode)(TAB_COUNT + 4))
#define TEST_HOTKEYS_TAB ((TabMode)(TAB_COUNT + 5))
#define TEST_RULES_TAB ((TabMode)(TAB_COUNT + 6))
#define TEST_APPS_TAB  ((TabMode)(TAB_COUNT + 7))

void filter_apps(AppData *app, const char *query);
void filter_workspaces(AppData *app, const char *query);
void filter_harpoon(AppData *app, const char *filter);
void filter_matching(AppData *app, const char *filter);
void filter_config(AppData *app, const char *filter);
void filter_hotkeys(AppData *app, const char *filter);
void filter_rules(AppData *app, const char *filter);
void reset_selection(AppData *app);
void preserve_selection(AppData *app);
void restore_selection(AppData *app);

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
    g_command_update_candidates_calls++;
    strncpy(g_last_command_candidates_text, text ? text : "", sizeof(g_last_command_candidates_text) - 1);
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

void exit_command_mode(AppData *app) {
    if (app) {
        app->command_mode.state = CMD_MODE_NORMAL;
        app->active_prefix_claim = '\0';
        if (app->mode_indicator)
            gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");
    }
}

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    g_enter_modal_calls++;
    if (app) {
        if (provider)
            app->current_tab = (TabMode)provider->tab_mode;
        app->command_mode.state = CMD_MODE_MODAL;
        if (app->entry)
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
        if (app->mode_indicator)
            gtk_label_set_text(GTK_LABEL(app->mode_indicator), "=");
    }
}

void cofi_exit_modal(AppData *app) {
    g_exit_modal_calls++;
    if (app) {
        app->command_mode.state = CMD_MODE_NORMAL;
        app->current_tab = app->prefix_origin_tab;
        if (app->mode_indicator)
            gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");
    }
}

gboolean cofi_handle_modal_key(AppData *app, GdkEventKey *event) {
    (void)event;
    (void)app;
    g_handle_modal_calls++;
    return g_modal_handler_returns;
}

const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) {
    if (prefix == '!' || prefix == '=')
        return &g_modal_prefix_stub;
    return NULL;
}

static void apps_on_tab_prefix_stub(AppData *app, char prefix) {
    if (!app) return;
    app->apps_mode = prefix == '$' ? APPS_MODE_PATH : APPS_MODE_DEFAULT;
}

const CofiTabProvider *cofi_get_provider_for_tab_prefix(char prefix) {
    g_tab_prefix_lookup_calls++;
    g_last_tab_prefix_lookup = prefix;
    if (prefix == '$' || prefix == '\\') {
        static CofiTabProvider apps_provider;
        memset(&apps_provider, 0, sizeof(apps_provider));
        apps_provider.tab_mode = TEST_APPS_TAB;
        apps_provider.id = "apps";
        apps_provider.tab_prefix_chars = "$\\";
        apps_provider.on_tab_prefix = apps_on_tab_prefix_stub;
        apps_provider.on_query_changed = filter_apps;
        return &apps_provider;
    }
    return NULL;
}

TabMode apps_tab_mode(void) {
    return TEST_APPS_TAB;
}

TabMode harpoon_tab_mode(void) {
    return TEST_HARPOON_TAB;
}

const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    if (g_provider_for_tab && g_provider_for_tab->tab_mode == tab_mode)
        return g_provider_for_tab;
    if (tab_mode == TEST_APPS_TAB) {
        static CofiTabProvider apps_provider;
        memset(&apps_provider, 0, sizeof(apps_provider));
        apps_provider.tab_mode = TEST_APPS_TAB;
        apps_provider.id = "apps";
        return &apps_provider;
    }
    return NULL;
}
int cofi_get_provider_id_for_tab(int tab_mode) { (void)tab_mode; return 0; }
int cofi_filtered_to_raw(int provider_id, int filtered_idx) {
    (void)provider_id;
    return filtered_idx;
}

WindowInfo *get_selected_window(AppData *app) {
    if (!app || app->current_tab != TAB_WINDOWS || app->filtered_count <= 0) return NULL;
    if (app->selection.window_index < 0 || app->selection.window_index >= app->filtered_count) return NULL;
    return &app->filtered[app->selection.window_index];
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

int get_selected_index(AppData *app) {
    return app ? app->selection.window_index : 0;
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

void sinks_switch_selected(AppData *app) { (void)app; g_sinks_switch_calls++; }
void sinks_filter(AppData *app, const char *filter) { (void)app; (void)filter; }
gboolean sinks_switch_name(AppData *app, const char *sink_name) { (void)app; (void)sink_name; return TRUE; }
gboolean sinks_assign_selected_slot(AppData *app, char slot_key) { (void)app; (void)slot_key; return FALSE; }
gboolean sinks_switch_slot(AppData *app, char slot_key) { (void)app; (void)slot_key; return FALSE; }
gboolean proc_signal_selected_with_modifiers(AppData *app, guint state) {
    (void)app;
    g_proc_signal_calls++;
    g_last_proc_signal_state = state;
    return TRUE;
}
void proc_filter(AppData *app, const char *filter) { (void)app; (void)filter; }

static CofiActionStatus mock_provider_enter_pressed(AppData *app, int filtered_idx, int raw_idx,
                                                    const char *entry_text, int modifier_state) {
    (void)app; (void)filtered_idx; (void)raw_idx; (void)entry_text;
    g_provider_enter_calls++;
    g_provider_enter_modifier_state = modifier_state;
    return COFI_NO_OP;
}

static CofiActionStatus mock_apps_enter_pressed(AppData *app, int filtered_idx, int raw_idx,
                                                const char *entry_text, int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    if (raw_idx < 0 || raw_idx >= app->filtered_apps_count) return COFI_NO_OP;
    apps_launch(&app->filtered_apps[raw_idx]);
    return COFI_HANDLED_HIDE;
}

static CofiActionStatus mock_workspaces_enter_pressed(AppData *app, int filtered_idx, int raw_idx,
                                                      const char *entry_text, int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    if (raw_idx < 0 || raw_idx >= app->filtered_workspace_count) return COFI_NO_OP;
    switch_to_desktop(app->display, app->filtered_workspaces[raw_idx].id);
    return COFI_HANDLED_HIDE;
}

static void mock_apps_query_changed(AppData *app, const char *query) {
    filter_apps(app, query);
    reset_selection(app);
}

static void mock_workspaces_query_changed(AppData *app, const char *query) {
    filter_workspaces(app, query);
    reset_selection(app);
}

static void mock_hotkeys_query_changed(AppData *app, const char *query) {
    filter_hotkeys(app, query);
    reset_selection(app);
}

static void mock_config_query_changed(AppData *app, const char *query) {
    filter_config(app, query);
    reset_selection(app);
}

static void mock_names_query_changed(AppData *app, const char *query) {
    filter_matching(app, query);
    reset_selection(app);
}

static void mock_harpoon_query_changed(AppData *app, const char *query) {
    filter_harpoon(app, query);
    reset_selection(app);
}

static void mock_rules_query_changed(AppData *app, const char *query) {
    filter_rules(app, query);
    reset_selection(app);
}

void switch_to_tab(AppData *app, TabMode target_tab) {
    if (app->current_tab == TEST_APPS_TAB && target_tab != TEST_APPS_TAB)
        app->apps_mode = APPS_MODE_DEFAULT;
    app->current_tab = target_tab;
}

/* required but not exercised here */
int get_number_of_desktops(Display *display) { (void)display; return 0; }
void assign_workspace_slots(AppData *app) { (void)app; }
Window get_workspace_slot_window(const WorkspaceSlotManager *manager, int slot) { (void)manager; (void)slot; return 0; }
Window get_slot_window(const HarpoonManager *manager, int slot) { (void)manager; (void)slot; return 0; }
int get_window_slot(const HarpoonManager *manager, Window id) { (void)manager; (void)id; return -1; }
void unassign_slot(HarpoonManager *manager, int slot) { (void)manager; (void)slot; }
void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window) { (void)manager; (void)slot; (void)window; }
int harpoon_gc_unreferenced_match_entries(HarpoonManager *manager) { (void)manager; return 0; }
void save_harpoon_slots(const HarpoonManager *manager) { (void)manager; }
int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) {
    (void)manager; (void)match_id; return -1;
}
void save_config(const CofiConfig *config) { (void)config; }
void show_name_edit_overlay(AppData *app) { (void)app; }
void show_name_delete_overlay(AppData *app, const char *custom_name, int manager_index) {
    (void)app; (void)custom_name; (void)manager_index;
}
int match_entry_find_index_by_window(const MatchEntryManager *manager, Window id) { (void)manager; (void)id; return -1; }
int match_entry_find_index_by_custom_name(const MatchEntryManager *manager, const char *custom_name) { (void)manager; (void)custom_name; return -1; }
void match_entry_delete_custom_name(MatchEntryManager *manager, int index) { (void)manager; (void)index; }
void save_match_entries(const MatchEntryManager *manager) { (void)manager; }
int matching_capture_or_get(MatchEntryManager *manager, WindowInfo *windows, int window_count, const WindowInfo *w) {
    (void)manager; (void)windows; (void)window_count; (void)w; return -1;
}
gboolean get_window_geometry(Display *display, Window window, int *x, int *y, int *width, int *height) {
    (void)display; (void)window; (void)x; (void)y; (void)width; (void)height; return FALSE;
}
int get_window_desktop(Display *display, Window window) { (void)display; (void)window; return 0; }
void move_window_to_desktop(Display *display, Window window, int desktop_index) {
    (void)display; (void)window; (void)desktop_index;
}
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
int replay_all_rules_against_open_windows(AppData *app) { (void)app; return 0; }
gboolean replay_selected_filtered_rule(AppData *app) { (void)app; return TRUE; }

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
void filter_matching(AppData *app, const char *filter) {
    (void)app;
    g_filter_names_calls++;
    strncpy(g_last_filter_names, filter ? filter : "", sizeof(g_last_filter_names) - 1);
}

MatchEntry *matching_selected_entry(AppData *app) { (void)app; return NULL; }
int matching_selected_manager_index(AppData *app) { (void)app; return -1; }
void matching_select_custom_name(AppData *app, const char *custom_name) { (void)app; (void)custom_name; }

void filter_config(AppData *app, const char *filter) {
    (void)app;
    g_filter_config_calls++;
    strncpy(g_last_filter_config, filter ? filter : "", sizeof(g_last_filter_config) - 1);
}

ConfigEntry *config_selected_entry(AppData *app) {
    if (!app || app->filtered_config_count <= 0) return NULL;
    return &app->filtered_config[0];
}

void config_select_key(AppData *app, const char *key) {
    (void)app;
    (void)key;
}

int config_entry_allows_edit(const ConfigEntry *entry) {
    return entry && (entry->type == CONFIG_TYPE_INT ||
                     entry->type == CONFIG_TYPE_STRING);
}

void filter_hotkeys(AppData *app, const char *filter) {
    (void)app;
    g_filter_hotkeys_calls++;
    strncpy(g_last_filter_hotkeys, filter ? filter : "", sizeof(g_last_filter_hotkeys) - 1);
}

HotkeyBinding *hotkeys_selected_binding(AppData *app, int *master_idx_out) {
    if (master_idx_out) *master_idx_out = -1;
    if (!app || app->filtered_hotkeys_count <= 0) return NULL;
    return &app->filtered_hotkeys[0];
}

void hotkeys_select_key(AppData *app, const char *key) {
    (void)app;
    (void)key;
}
void filter_rules(AppData *app, const char *filter) {
    (void)app;
    g_filter_rules_calls++;
    strncpy(g_last_filter_rules, filter ? filter : "", sizeof(g_last_filter_rules) - 1);
}

Rule *rules_selected_rule(AppData *app) {
    if (!app || app->filtered_rules_count <= 0) return NULL;
    return &app->filtered_rules[0];
}

int rules_selected_config_index(AppData *app) {
    (void)app;
    return 0;
}

void rules_select_config_index(AppData *app, int config_index) {
    (void)app;
    (void)config_index;
}
void filter_apps(AppData *app, const char *query) {
    (void)app;
    g_filter_apps_calls++;
    strncpy(g_last_filter_apps, query ? query : "", sizeof(g_last_filter_apps) - 1);
}

void reset_selection(AppData *app) { (void)app; g_reset_selection_calls++; }
void preserve_selection(AppData *app) { (void)app; }
void restore_selection(AppData *app) { (void)app; }
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
    g_sinks_switch_calls = 0;
    g_proc_signal_calls = 0;
    g_last_proc_signal_state = 0;
    g_provider_enter_calls = 0;
    g_provider_enter_modifier_state = 0;
    g_provider_for_tab = NULL;
    g_workspace_switch_state = 0;
    g_highlight_calls = 0;
    g_last_highlight_window = 0;
    g_last_windows_query[0] = '\0';

    g_overlay_active = FALSE;
    g_handle_overlay_calls = 0;
    g_overlay_handler_returns = FALSE;
    g_handle_command_calls = 0;
    g_command_handler_returns = FALSE;
    g_command_update_candidates_calls = 0;
    g_last_command_candidates_text[0] = '\0';
    g_handle_modal_calls = 0;
    g_modal_handler_returns = FALSE;
    g_handle_tab_switching_calls = 0;
    g_tab_switching_returns = FALSE;
    g_enter_command_mode_calls = 0;
    g_enter_modal_calls = 0;
    g_exit_modal_calls = 0;
    g_move_selection_up_calls = 0;
    g_move_selection_down_calls = 0;

    g_filter_windows_calls = 0;
    g_filter_workspaces_calls = 0;
    g_filter_harpoon_calls = 0;
    g_filter_names_calls = 0;
    g_filter_config_calls = 0;
    g_filter_hotkeys_calls = 0;
    g_filter_rules_calls = 0;
    g_filter_apps_calls = 0;
    g_last_filter_windows[0] = '\0';
    g_last_filter_workspaces[0] = '\0';
    g_last_filter_harpoon[0] = '\0';
    g_last_filter_names[0] = '\0';
    g_last_filter_config[0] = '\0';
    g_last_filter_hotkeys[0] = '\0';
    g_last_filter_rules[0] = '\0';
    g_last_filter_apps[0] = '\0';
    g_reset_selection_calls = 0;
    g_update_display_calls = 0;
    g_tab_prefix_lookup_calls = 0;
    g_last_tab_prefix_lookup = '\0';
    memset(&g_modal_prefix_stub, 0, sizeof(g_modal_prefix_stub));
    g_modal_prefix_stub.tab_mode = TAB_COUNT + 1;
    g_modal_prefix_stub.id = "calc";
    g_modal_prefix_stub.prefix_char = '=';
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->entry = gtk_entry_new();
    app->mode_indicator = gtk_label_new(">");
    app->window_visible = TRUE;
    app->command_mode.state = CMD_MODE_NORMAL;
    app->prefix_origin_tab = TAB_WINDOWS;
    app->active_prefix_claim = '\0';
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

static void test_escape_provider_tab_hides(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    CofiTabProvider provider;
    memset(&provider, 0, sizeof(provider));
    provider.tab_mode = TEST_CONFIG_TAB;
    g_provider_for_tab = &provider;

    app.current_tab = TEST_CONFIG_TAB;
    app.prefix_origin_tab = TAB_WINDOWS;
    app.tab_visibility[TEST_CONFIG_TAB] = TAB_VIS_SURFACED;

    GdkEventKey ev = make_key(GDK_KEY_Escape, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Escape on provider tab handled", handled == TRUE);
    ASSERT_TRUE("Escape on provider tab hides window", app.window_visible == FALSE && g_hide_calls == 1);
    ASSERT_TRUE("Escape on provider tab does not switch origin", g_switch_calls == 0);
}

static void test_escape_harpoon_pending_delete_cancels_only(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TEST_HARPOON_TAB;
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

    app.current_tab = TEST_APPS_TAB;
    app.filtered_apps_count = 1;
    app.selection.provider_index = 0;
    strcpy(app.filtered_apps[0].name, "Firefox");
    CofiTabProvider provider;
    memset(&provider, 0, sizeof(provider));
    provider.tab_mode = TEST_APPS_TAB;
    provider.on_enter_pressed = mock_apps_enter_pressed;
    g_provider_for_tab = &provider;

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

    app.current_tab = TEST_WORKSPACES_TAB;
    app.filtered_workspace_count = 1;
    app.selection.provider_index = 0;
    app.filtered_workspaces[0].id = 3;
    strcpy(app.filtered_workspaces[0].name, "WS4");
    CofiTabProvider provider;
    memset(&provider, 0, sizeof(provider));
    provider.tab_mode = TEST_WORKSPACES_TAB;
    provider.on_enter_pressed = mock_workspaces_enter_pressed;
    g_provider_for_tab = &provider;

    GdkEventKey ev = make_key(GDK_KEY_Return, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Return on Workspaces handled", handled == TRUE);
    ASSERT_TRUE("Return on Workspaces switches selected workspace",
                g_switch_calls == 1 && g_last_switched_desktop == 3);
    ASSERT_TRUE("Return on Workspaces hides window", g_hide_calls == 1 && app.window_visible == FALSE);
}

static void test_return_proc_routes_signal_by_modifier(void) {
    AppData app;
    CofiTabProvider provider;
    init_app(&app);
    reset_captures();
    app.current_tab = (TabMode)(TAB_COUNT + 1);
    memset(&provider, 0, sizeof(provider));
    provider.tab_mode = app.current_tab;
    provider.on_enter_pressed = mock_provider_enter_pressed;
    g_provider_for_tab = &provider;

    GdkEventKey ev = make_key(GDK_KEY_Return, GDK_SHIFT_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Return on Proc handled", handled == TRUE);
    ASSERT_TRUE("Return on Proc routes to provider enter with modifiers",
                g_provider_enter_calls == 1 &&
                g_provider_enter_modifier_state == (int)GDK_SHIFT_MASK);
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

static void test_exclam_enters_run_modal_with_empty_entry(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.command_mode.state = CMD_MODE_NORMAL;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");

    GdkEventKey ev = make_key(GDK_KEY_exclam, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Exclam handled on empty entry", handled == TRUE);
    ASSERT_TRUE("Exclam enters modal on empty entry",
                app.command_mode.state == CMD_MODE_MODAL && g_enter_modal_calls == 1);
    ASSERT_TRUE("Exclam sets active prefix claim '!'", app.active_prefix_claim == '!');

    init_app(&app);
    reset_captures();
    app.command_mode.state = CMD_MODE_NORMAL;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "qemu");
    GdkEventKey ev2 = make_key(GDK_KEY_exclam, 0);
    gboolean handled2 = on_key_press(NULL, &ev2, &app);
    ASSERT_TRUE("Exclam not handled when entry non-empty", handled2 == FALSE);
    ASSERT_TRUE("Exclam does not enter modal on non-empty entry",
                app.command_mode.state == CMD_MODE_NORMAL && g_enter_modal_calls == 0);
}

static void test_colon_enters_command_mode_with_nonempty_filter(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.command_mode.state = CMD_MODE_NORMAL;
    app.current_tab = TAB_WINDOWS;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "terminal");

    GdkEventKey ev = make_key(GDK_KEY_colon, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Colon handled with non-empty filter", handled == TRUE);
    ASSERT_TRUE("Colon enters command mode with non-empty filter",
                app.command_mode.state == CMD_MODE_COMMAND &&
                g_enter_command_mode_calls == 1);
    ASSERT_TRUE("Colon sets active prefix claim ':'", app.active_prefix_claim == ':');
    ASSERT_TRUE("Colon does not append to filter text",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "terminal") == 0);
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
    ASSERT_TRUE("Overlay active bypasses command/modal handlers", g_handle_command_calls == 0 && g_handle_modal_calls == 0);
}

static void test_command_mode_dispatch_precedence(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.command_mode.state = CMD_MODE_COMMAND;
    app.active_prefix_claim = ':';
    g_command_handler_returns = TRUE;

    GdkEventKey ev = make_key(GDK_KEY_colon, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("CMD_MODE_COMMAND routes to command handler", handled == TRUE && g_handle_command_calls == 1);
    ASSERT_TRUE("CMD_MODE_COMMAND does not hit normal dispatch", g_enter_command_mode_calls == 0 && g_activate_calls == 0);
}


static void test_calc_mode_dispatch_precedence(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.command_mode.state = CMD_MODE_MODAL;
    g_modal_handler_returns = TRUE;

    GdkEventKey ev = make_key(GDK_KEY_Return, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("CMD_MODE_MODAL routes to modal handler", handled == TRUE && g_handle_modal_calls == 1);
    ASSERT_TRUE("CMD_MODE_MODAL does not hit navigation dispatch", g_activate_calls == 0 && g_hide_calls == 0);
}

static void test_on_entry_changed_routes_per_tab_filters(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    TabMode tabs[] = {
        TAB_WINDOWS, TEST_WORKSPACES_TAB, TEST_HARPOON_TAB,
        TEST_MATCHING_TAB, TEST_CONFIG_TAB, TEST_HOTKEYS_TAB, TEST_RULES_TAB, TEST_APPS_TAB
    };

    for (int i = 0; i < 8; i++) {
        g_filter_windows_calls = g_filter_workspaces_calls = g_filter_harpoon_calls = 0;
        g_filter_names_calls = g_filter_config_calls = g_filter_hotkeys_calls = 0;
        g_filter_rules_calls = g_filter_apps_calls = 0;
        g_reset_selection_calls = 0;
        g_update_display_calls = 0;

        CofiTabProvider apps_provider;
        memset(&apps_provider, 0, sizeof(apps_provider));
        apps_provider.tab_mode = TEST_APPS_TAB;
        apps_provider.on_query_changed = mock_apps_query_changed;

        CofiTabProvider workspaces_provider;
        memset(&workspaces_provider, 0, sizeof(workspaces_provider));
        workspaces_provider.tab_mode = TEST_WORKSPACES_TAB;
        workspaces_provider.on_query_changed = mock_workspaces_query_changed;

        CofiTabProvider hotkeys_provider;
        memset(&hotkeys_provider, 0, sizeof(hotkeys_provider));
        hotkeys_provider.tab_mode = TEST_HOTKEYS_TAB;
        hotkeys_provider.on_query_changed = mock_hotkeys_query_changed;

        CofiTabProvider config_provider;
        memset(&config_provider, 0, sizeof(config_provider));
        config_provider.tab_mode = TEST_CONFIG_TAB;
        config_provider.on_query_changed = mock_config_query_changed;

        CofiTabProvider matching_provider;
        memset(&matching_provider, 0, sizeof(matching_provider));
        matching_provider.tab_mode = TEST_MATCHING_TAB;
        matching_provider.on_query_changed = mock_names_query_changed;

        CofiTabProvider rules_provider;
        memset(&rules_provider, 0, sizeof(rules_provider));
        rules_provider.tab_mode = TEST_RULES_TAB;
        rules_provider.on_query_changed = mock_rules_query_changed;

        CofiTabProvider harpoon_provider;
        memset(&harpoon_provider, 0, sizeof(harpoon_provider));
        harpoon_provider.tab_mode = TEST_HARPOON_TAB;
        harpoon_provider.on_query_changed = mock_harpoon_query_changed;

        if (tabs[i] == TEST_APPS_TAB) {
            g_provider_for_tab = &apps_provider;
        } else if (tabs[i] == TEST_WORKSPACES_TAB) {
            g_provider_for_tab = &workspaces_provider;
        } else if (tabs[i] == TEST_HARPOON_TAB) {
            g_provider_for_tab = &harpoon_provider;
        } else if (tabs[i] == TEST_MATCHING_TAB) {
            g_provider_for_tab = &matching_provider;
        } else if (tabs[i] == TEST_CONFIG_TAB) {
            g_provider_for_tab = &config_provider;
        } else if (tabs[i] == TEST_HOTKEYS_TAB) {
            g_provider_for_tab = &hotkeys_provider;
        } else if (tabs[i] == TEST_RULES_TAB) {
            g_provider_for_tab = &rules_provider;
        } else {
            g_provider_for_tab = NULL;
        }
        app.current_tab = tabs[i];
        gtk_entry_set_text(GTK_ENTRY(app.entry), "query");
        on_entry_changed(GTK_ENTRY(app.entry), &app);

        int sum = g_filter_windows_calls + g_filter_workspaces_calls + g_filter_harpoon_calls +
                  g_filter_names_calls + g_filter_config_calls + g_filter_hotkeys_calls +
                  g_filter_rules_calls + g_filter_apps_calls;

        ASSERT_TRUE("on_entry_changed calls exactly one tab filter", sum == 1);
        ASSERT_TRUE("on_entry_changed calls reset_selection", g_reset_selection_calls == 1);
        ASSERT_TRUE("on_entry_changed calls update_display", g_update_display_calls == 1);

        ASSERT_TRUE("WINDOWS filter routing",
                    tabs[i] != TAB_WINDOWS || (g_filter_windows_calls == 1 && strcmp(g_last_filter_windows, "query") == 0));
        ASSERT_TRUE("WORKSPACES filter routing",
                    tabs[i] != TEST_WORKSPACES_TAB || (g_filter_workspaces_calls == 1 && strcmp(g_last_filter_workspaces, "query") == 0));
        ASSERT_TRUE("HARPOON filter routing",
                    tabs[i] != TEST_HARPOON_TAB || (g_filter_harpoon_calls == 1 && strcmp(g_last_filter_harpoon, "query") == 0));
        ASSERT_TRUE("NAMES filter routing",
                    tabs[i] != TEST_MATCHING_TAB || (g_filter_names_calls == 1 && strcmp(g_last_filter_names, "query") == 0));
        ASSERT_TRUE("CONFIG filter routing",
                    tabs[i] != TEST_CONFIG_TAB || (g_filter_config_calls == 1 && strcmp(g_last_filter_config, "query") == 0));
        ASSERT_TRUE("HOTKEYS filter routing",
                    tabs[i] != TEST_HOTKEYS_TAB || (g_filter_hotkeys_calls == 1 && strcmp(g_last_filter_hotkeys, "query") == 0));
        ASSERT_TRUE("RULES filter routing",
                    tabs[i] != TEST_RULES_TAB || (g_filter_rules_calls == 1 && strcmp(g_last_filter_rules, "query") == 0));
        ASSERT_TRUE("APPS filter routing",
                    tabs[i] != TEST_APPS_TAB || (g_filter_apps_calls == 1 && strcmp(g_last_filter_apps, "query") == 0));
    }
}

static void test_on_entry_changed_leading_colon_claims_command_mode(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TEST_HARPOON_TAB;
    gtk_entry_set_text(GTK_ENTRY(app.entry), ":set");

    on_entry_changed(GTK_ENTRY(app.entry), &app);

    ASSERT_TRUE("Leading ':' enters command mode from entry change",
                app.command_mode.state == CMD_MODE_COMMAND && g_enter_command_mode_calls == 1);
    ASSERT_TRUE("Leading ':' stores origin tab", app.prefix_origin_tab == TEST_HARPOON_TAB);
}

static void test_on_entry_changed_provider_prefix_preserves_remainder(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TAB_WINDOWS;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "=1.3+2");

    on_entry_changed(GTK_ENTRY(app.entry), &app);

    ASSERT_TRUE("Leading '=' enters calc modal from entry change",
                app.command_mode.state == CMD_MODE_MODAL && g_enter_modal_calls == 1);
    ASSERT_TRUE("Leading '=' stores origin tab", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("Leading '=' preserves expression remainder",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "1.3+2") == 0);
}


static void test_on_entry_changed_prefix_tabs_claim_and_restore_origin(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TEST_HOTKEYS_TAB;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "$term");
    on_entry_changed(GTK_ENTRY(app.entry), &app);

    ASSERT_TRUE("Leading '$' claims Apps tab", app.current_tab == TEST_APPS_TAB);
    ASSERT_TRUE("Leading '$' stores origin tab once", app.prefix_origin_tab == TEST_HOTKEYS_TAB);
    ASSERT_TRUE("Leading '$' marks active claim", app.active_prefix_claim == '$');

    gtk_entry_set_text(GTK_ENTRY(app.entry), "");
    on_entry_changed(GTK_ENTRY(app.entry), &app);
    ASSERT_TRUE("Backspace to empty restores origin tab", app.current_tab == TEST_HOTKEYS_TAB);
    ASSERT_TRUE("Backspace to empty clears claim", app.active_prefix_claim == '\0');
}

static void test_on_entry_changed_placeholder_prefixes_stay_claimed_until_empty(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    /* Set modal state directly (entry-change path no longer fires for =) */
    app.current_tab = TEST_CONFIG_TAB;
    app.command_mode.state = CMD_MODE_MODAL;
    app.active_prefix_claim = '=';
    app.prefix_origin_tab = TEST_CONFIG_TAB;
    g_enter_modal_calls = 1;
    /* In CMD_MODE_MODAL, further entry changes do not re-trigger cofi_enter_modal */
    gtk_entry_set_text(GTK_ENTRY(app.entry), "17+4");
    on_entry_changed(GTK_ENTRY(app.entry), &app);
    ASSERT_TRUE("In CMD_MODE_MODAL, entry change does not re-enter modal",
                g_enter_modal_calls == 1 && app.command_mode.state == CMD_MODE_MODAL);

    /* Reset to NORMAL for independent '>' test */
    app.command_mode.state = CMD_MODE_NORMAL;
    app.active_prefix_claim = '\0';
    gtk_entry_set_text(GTK_ENTRY(app.entry), ">go");
    on_entry_changed(GTK_ENTRY(app.entry), &app);
    ASSERT_TRUE("Leading '>' also claims Windows tab", app.current_tab == TAB_WINDOWS && app.active_prefix_claim == '>');
}

static void test_gt_prefix_is_core_claim(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TEST_APPS_TAB;

    gtk_entry_set_text(GTK_ENTRY(app.entry), ">term");
    on_entry_changed(GTK_ENTRY(app.entry), &app);

    ASSERT_TRUE("Leading '>' switches to core Windows tab", app.current_tab == TAB_WINDOWS);
    ASSERT_TRUE("Leading '>' is not resolved through provider tab prefixes",
                g_tab_prefix_lookup_calls == 0);
}

static void test_tab_key_clears_prefix_claim_before_tab_switching(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.active_prefix_claim = '$';
    app.current_tab = TEST_APPS_TAB;
    g_tab_switching_returns = FALSE;

    GdkEventKey ev = make_key(GDK_KEY_Tab, 0);
    on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Manual Tab clears active prefix claim", app.active_prefix_claim == '\0');
}

static void test_backslash_cross_tab_enters_apps_default_mode(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TAB_WINDOWS;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");

    GdkEventKey ev = make_key(GDK_KEY_backslash, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("backslash cross-tab handled", handled == TRUE);
    ASSERT_TRUE("backslash cross-tab enters Apps", app.current_tab == TEST_APPS_TAB);
    ASSERT_TRUE("backslash cross-tab enters default mode", app.apps_mode == APPS_MODE_DEFAULT);
}

static void test_dollar_cross_tab_enters_apps_path_mode(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TAB_WINDOWS;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");

    GdkEventKey ev = make_key(GDK_KEY_dollar, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("dollar cross-tab handled", handled == TRUE);
    ASSERT_TRUE("dollar cross-tab enters Apps", app.current_tab == TEST_APPS_TAB);
    ASSERT_TRUE("dollar cross-tab enters PATH mode", app.apps_mode == APPS_MODE_PATH);
}

static void test_backslash_same_tab_resets_to_default(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TEST_APPS_TAB;
    app.apps_mode = APPS_MODE_PATH;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");

    GdkEventKey ev = make_key(GDK_KEY_backslash, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("backslash same-tab handled", handled == TRUE);
    ASSERT_TRUE("backslash same-tab stays in Apps", app.current_tab == TEST_APPS_TAB);
    ASSERT_TRUE("backslash same-tab enters default mode", app.apps_mode == APPS_MODE_DEFAULT);
    ASSERT_TRUE("backslash same-tab clears entry",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "") == 0);
    ASSERT_TRUE("backslash same-tab sets indicator",
                gtk_label_get_text(GTK_LABEL(app.mode_indicator))[0] == '\\' &&
                gtk_label_get_text(GTK_LABEL(app.mode_indicator))[1] == '\0');
}

static void test_dollar_same_tab_switches_to_path(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TEST_APPS_TAB;
    app.apps_mode = APPS_MODE_DEFAULT;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");

    GdkEventKey ev = make_key(GDK_KEY_dollar, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("dollar same-tab handled", handled == TRUE);
    ASSERT_TRUE("dollar same-tab stays in Apps", app.current_tab == TEST_APPS_TAB);
    ASSERT_TRUE("dollar same-tab enters PATH mode", app.apps_mode == APPS_MODE_PATH);
    ASSERT_TRUE("dollar same-tab clears entry",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "") == 0);
    ASSERT_TRUE("dollar same-tab sets indicator",
                strcmp(gtk_label_get_text(GTK_LABEL(app.mode_indicator)), "$") == 0);
}

static void test_switch_away_from_apps_resets_mode(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TEST_APPS_TAB;
    app.apps_mode = APPS_MODE_PATH;

    switch_to_tab(&app, TEST_WORKSPACES_TAB);

    ASSERT_TRUE("switch away from Apps resets mode", app.apps_mode == APPS_MODE_DEFAULT);
}

static void test_command_mode_prefix_exits_to_modal(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TAB_WINDOWS;
    app.prefix_origin_tab = TAB_WINDOWS;
    app.active_prefix_claim = ':';
    app.command_mode.state = CMD_MODE_COMMAND;
    gtk_label_set_text(GTK_LABEL(app.mode_indicator), ":");
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");

    GdkEventKey ev = make_key(GDK_KEY_equal, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("COMMAND -> MODAL handled", handled == TRUE);
    ASSERT_TRUE("COMMAND -> MODAL state", app.command_mode.state == CMD_MODE_MODAL);
    ASSERT_TRUE("COMMAND -> MODAL indicator",
                gtk_label_get_text(GTK_LABEL(app.mode_indicator))[0] == '=');
    ASSERT_TRUE("COMMAND -> MODAL origin preserved", app.prefix_origin_tab == TAB_WINDOWS);
}

static void test_command_mode_prefix_exits_to_tab_claim(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TAB_WINDOWS;
    app.active_prefix_claim = ':';
    app.command_mode.state = CMD_MODE_COMMAND;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");

    GdkEventKey ev = make_key(GDK_KEY_backslash, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("COMMAND -> APPS handled", handled == TRUE);
    ASSERT_TRUE("COMMAND -> APPS state is NORMAL", app.command_mode.state == CMD_MODE_NORMAL);
    ASSERT_TRUE("COMMAND -> APPS tab", app.current_tab == TEST_APPS_TAB);
    ASSERT_TRUE("COMMAND -> APPS mode DEFAULT", app.apps_mode == APPS_MODE_DEFAULT);
}

static void test_command_mode_same_prefix_colon_noop(void) {
    AppData app;
    init_app(&app);
    reset_captures();
    app.current_tab = TAB_WINDOWS;
    app.active_prefix_claim = ':';
    app.command_mode.state = CMD_MODE_COMMAND;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "");
    g_enter_command_mode_calls = 0;
    g_enter_modal_calls = 0;
    g_command_handler_returns = TRUE;

    GdkEventKey ev = make_key(GDK_KEY_colon, 0);
    gboolean handled = on_key_press(NULL, &ev, &app);

    /* colon in command mode: gate does NOT fire (same-prefix guard),
       falls through to handle_command_key which processes it */
    ASSERT_TRUE("same-prefix colon handled by command mode", handled == TRUE);
    ASSERT_TRUE("same-prefix colon in COMMAND stays COMMAND",
                app.command_mode.state == CMD_MODE_COMMAND);
    ASSERT_TRUE("same-prefix colon no re-enter modal", g_enter_modal_calls == 0);
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
    test_escape_provider_tab_hides();
    test_escape_harpoon_pending_delete_cancels_only();
    test_return_windows_activates_selected_and_hides();
    test_return_apps_launches_selected_and_hides();
    test_return_workspaces_switches_desktop_and_hides();
    test_return_proc_routes_signal_by_modifier();
    test_up_arrow_moves_selection();
    test_down_arrow_moves_selection();
    test_ctrl_k_matches_up_behavior();
    test_ctrl_j_matches_down_behavior();
    test_alt_tab_moves_selection_up_on_windows_tab();
    test_alt_shift_tab_moves_selection_down_on_windows_tab();
    test_period_empty_query_calls_repeat();
    test_colon_enters_command_mode();
    test_exclam_enters_run_modal_with_empty_entry();
    test_colon_enters_command_mode_with_nonempty_filter();
    test_overlay_dispatch_precedence();
    test_command_mode_dispatch_precedence();
    test_calc_mode_dispatch_precedence();
    test_on_entry_changed_routes_per_tab_filters();
    test_on_entry_changed_leading_colon_claims_command_mode();
    test_on_entry_changed_provider_prefix_preserves_remainder();
    test_on_entry_changed_prefix_tabs_claim_and_restore_origin();
    test_on_entry_changed_placeholder_prefixes_stay_claimed_until_empty();
    test_gt_prefix_is_core_claim();
    test_tab_key_clears_prefix_claim_before_tab_switching();
    test_backslash_cross_tab_enters_apps_default_mode();
    test_dollar_cross_tab_enters_apps_path_mode();
    test_backslash_same_tab_resets_to_default();
    test_dollar_same_tab_switches_to_path();
    test_switch_away_from_apps_resets_mode();
    test_command_mode_prefix_exits_to_modal();
    test_command_mode_prefix_exits_to_tab_claim();
    test_command_mode_same_prefix_colon_noop();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
