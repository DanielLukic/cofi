#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../src/app_data.h"
#include "../src/key_handler.h"

/*
 * Testability strategy:
 * Include key_handler.c directly and stub cross-module deps.
 * Stubs capture full arguments/context so tests assert exact target routing.
 */

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_show_name_edit_calls;
static int g_last_name_edit_index;
static NamedWindow g_last_name_edit_named;

static int g_show_harpoon_delete_calls;
static int g_last_harpoon_delete_slot;

static int g_show_harpoon_edit_calls;
static int g_last_harpoon_edit_slot;

static int g_save_config_calls;
static int g_regrab_hotkeys_calls;
static int g_save_hotkey_config_calls;
static int g_update_display_calls;
static int g_save_named_windows_calls;
static int g_cleanup_hotkeys_calls;

static int g_show_overlay_calls;
static OverlayType g_last_overlay_type;
static void *g_last_overlay_data;
static char g_last_overlay_config_key[64];
static char g_last_overlay_hotkey_key[64];

static int g_get_next_enum_calls;
static char g_last_get_next_enum_key[64];
static char g_last_get_next_enum_value[64];

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

/* --- Stubs required by key_handler.c --- */
gboolean is_overlay_active(AppData *app) { (void)app; return FALSE; }
gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event) { (void)app; (void)event; return FALSE; }
gboolean handle_command_key(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }
void command_update_candidates(CommandMode *cmd, const char *text) { (void)cmd; (void)text; }
gboolean handle_run_key(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }
gboolean handle_tab_switching(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }

void enter_command_mode(AppData *app) { (void)app; }
void enter_run_mode(AppData *app, const char *prefill_command) { (void)app; (void)prefill_command; }
void handle_run_entry_changed(GtkEntry *entry, AppData *app) { (void)entry; (void)app; }

WindowInfo *get_selected_window(AppData *app) { (void)app; return NULL; }
WorkspaceInfo *get_selected_workspace(AppData *app) { (void)app; return NULL; }
void move_selection_up(AppData *app) { (void)app; }
void move_selection_down(AppData *app) { (void)app; }
void handle_repeat_key(AppData *app) { (void)app; }
void store_last_windows_query(AppData *app, const char *query) { (void)app; (void)query; }
void set_workspace_switch_state(int state) { (void)state; }
void activate_window(Display *display, Window window_id) { (void)display; (void)window_id; }
void highlight_window(AppData *app, Window window_id) { (void)app; (void)window_id; }
void hide_window(AppData *app) { (void)app; }
void switch_to_desktop(Display *display, int desktop) { (void)display; (void)desktop; }
int get_number_of_desktops(Display *display) { (void)display; return 0; }
void assign_workspace_slots(AppData *app) { (void)app; }
Window get_workspace_slot_window(const WorkspaceSlotManager *manager, int slot) { (void)manager; (void)slot; return 0; }
Window get_slot_window(const HarpoonManager *manager, int slot) { (void)manager; (void)slot; return 0; }
int get_window_slot(const HarpoonManager *manager, Window id) { (void)manager; (void)id; return -1; }
void unassign_slot(HarpoonManager *manager, int slot) { (void)manager; (void)slot; }
void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window) { (void)manager; (void)slot; (void)window; }
void save_harpoon_slots(const HarpoonManager *manager) { (void)manager; }

void save_config(const CofiConfig *config) { (void)config; g_save_config_calls++; }

void update_display(AppData *app) { (void)app; g_update_display_calls++; }

void show_name_edit_overlay(AppData *app) {
    g_show_name_edit_calls++;
    g_last_name_edit_index = app ? app->selection.names_index : -1;
    if (app && app->selection.names_index >= 0 && app->selection.names_index < app->filtered_names_count) {
        g_last_name_edit_named = app->filtered_names[app->selection.names_index];
    } else {
        memset(&g_last_name_edit_named, 0, sizeof(g_last_name_edit_named));
    }
}

int find_named_window_by_name(const NamedWindowManager *manager, const char *custom_name) {
    if (!manager || !custom_name) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->entries[i].custom_name, custom_name) == 0) {
            return i;
        }
    }
    return -1;
}

void delete_custom_name(NamedWindowManager *manager, int index) {
    if (!manager || index < 0 || index >= manager->count) return;
    for (int i = index; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    manager->count--;
}

void save_named_windows(const NamedWindowManager *manager) { (void)manager; g_save_named_windows_calls++; }

void filter_names(AppData *app, const char *filter) {
    (void)filter;
    app->filtered_names_count = app->names.count;
    for (int i = 0; i < app->names.count; i++) {
        app->filtered_names[i] = app->names.entries[i];
    }
}

void show_harpoon_delete_overlay(AppData *app, int slot) {
    (void)app;
    g_show_harpoon_delete_calls++;
    g_last_harpoon_delete_slot = slot;
}

void show_harpoon_edit_overlay(AppData *app, int slot) {
    (void)app;
    g_show_harpoon_edit_calls++;
    g_last_harpoon_edit_slot = slot;
}

const char *get_next_enum_value(const char *key, const char *current_value) {
    g_get_next_enum_calls++;
    strncpy(g_last_get_next_enum_key, key ? key : "", sizeof(g_last_get_next_enum_key) - 1);
    strncpy(g_last_get_next_enum_value, current_value ? current_value : "", sizeof(g_last_get_next_enum_value) - 1);

    if (key && strcmp(key, "digit_slot_mode") == 0) {
        if (current_value && strcmp(current_value, "default") == 0) return "per-workspace";
        if (current_value && strcmp(current_value, "per-workspace") == 0) return "workspaces";
        return "default";
    }
    return NULL;
}

int apply_config_setting(CofiConfig *config, const char *key, const char *value, char *err_buf, size_t err_size) {
    (void)err_buf;
    (void)err_size;

    if (!config || !key || !value) return 0;

    if (strcmp(key, "close_on_focus_loss") == 0) {
        config->close_on_focus_loss = (strcmp(value, "true") == 0) ? 1 : 0;
        return 1;
    }

    if (strcmp(key, "digit_slot_mode") == 0) {
        if (strcmp(value, "default") == 0) config->digit_slot_mode = DIGIT_MODE_DEFAULT;
        else if (strcmp(value, "per-workspace") == 0) config->digit_slot_mode = DIGIT_MODE_PER_WORKSPACE;
        else if (strcmp(value, "workspaces") == 0) config->digit_slot_mode = DIGIT_MODE_WORKSPACES;
        else return 0;
        return 1;
    }

    return 0;
}

void filter_config(AppData *app, const char *filter) {
    (void)filter;

    if (app->selection.config_index >= 0 && app->selection.config_index < app->filtered_config_count) {
        ConfigEntry *entry = &app->filtered_config[app->selection.config_index];
        if (strcmp(entry->key, "close_on_focus_loss") == 0) {
            strcpy(entry->value, app->config.close_on_focus_loss ? "true" : "false");
            entry->type = CONFIG_TYPE_BOOL;
            return;
        }
        if (strcmp(entry->key, "digit_slot_mode") == 0) {
            const char *v = "default";
            if (app->config.digit_slot_mode == DIGIT_MODE_PER_WORKSPACE) v = "per-workspace";
            if (app->config.digit_slot_mode == DIGIT_MODE_WORKSPACES) v = "workspaces";
            strcpy(entry->value, v);
            entry->type = CONFIG_TYPE_ENUM;
            return;
        }
    }

    app->filtered_config_count = 1;
    strcpy(app->filtered_config[0].key, "close_on_focus_loss");
    strcpy(app->filtered_config[0].value, app->config.close_on_focus_loss ? "true" : "false");
    app->filtered_config[0].type = CONFIG_TYPE_BOOL;
}

void show_overlay(AppData *app, OverlayType type, void *data) {
    g_show_overlay_calls++;
    g_last_overlay_type = type;
    g_last_overlay_data = data;

    g_last_overlay_config_key[0] = '\0';
    g_last_overlay_hotkey_key[0] = '\0';

    if (!app) return;

    if (type == OVERLAY_CONFIG_EDIT &&
        app->selection.config_index >= 0 && app->selection.config_index < app->filtered_config_count) {
        strncpy(g_last_overlay_config_key,
                app->filtered_config[app->selection.config_index].key,
                sizeof(g_last_overlay_config_key) - 1);
    }

    if (type == OVERLAY_HOTKEY_EDIT &&
        app->selection.hotkeys_index >= 0 && app->selection.hotkeys_index < app->filtered_hotkeys_count) {
        strncpy(g_last_overlay_hotkey_key,
                app->filtered_hotkeys[app->selection.hotkeys_index].key,
                sizeof(g_last_overlay_hotkey_key) - 1);
    }
}

void cleanup_hotkeys(AppData *app) { (void)app; g_cleanup_hotkeys_calls++; }

int remove_hotkey_binding(HotkeyConfig *config, const char *key) {
    if (!config || !key) return 0;

    int idx = -1;
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->bindings[i].key, key) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return 0;

    for (int i = idx; i < config->count - 1; i++) {
        config->bindings[i] = config->bindings[i + 1];
    }
    config->count--;
    return 1;
}

int save_hotkey_config(const HotkeyConfig *config) { (void)config; g_save_hotkey_config_calls++; return 1; }
void regrab_hotkeys(AppData *app) { (void)app; g_regrab_hotkeys_calls++; }

void filter_hotkeys(AppData *app, const char *filter) {
    (void)filter;
    app->filtered_hotkeys_count = app->hotkey_config.count;
    for (int i = 0; i < app->hotkey_config.count; i++) {
        app->filtered_hotkeys[i] = app->hotkey_config.bindings[i];
    }
}

void filter_windows(AppData *app, const char *query) { (void)app; (void)query; }
void filter_workspaces(AppData *app, const char *query) { (void)app; (void)query; }
void filter_harpoon(AppData *app, const char *filter) { (void)app; (void)filter; }
void filter_apps(AppData *app, const char *query) { (void)app; (void)query; }
void reset_selection(AppData *app) { (void)app; }
void apps_launch(const AppEntry *entry) { (void)entry; }

#include "../src/key_handler.c"

static void reset_captures(void) {
    g_show_name_edit_calls = 0;
    g_last_name_edit_index = -1;
    memset(&g_last_name_edit_named, 0, sizeof(g_last_name_edit_named));

    g_show_harpoon_delete_calls = 0;
    g_last_harpoon_delete_slot = -1;
    g_show_harpoon_edit_calls = 0;
    g_last_harpoon_edit_slot = -1;

    g_save_config_calls = 0;
    g_regrab_hotkeys_calls = 0;
    g_save_hotkey_config_calls = 0;
    g_update_display_calls = 0;
    g_save_named_windows_calls = 0;
    g_cleanup_hotkeys_calls = 0;

    g_show_overlay_calls = 0;
    g_last_overlay_type = OVERLAY_NONE;
    g_last_overlay_data = NULL;
    g_last_overlay_config_key[0] = '\0';
    g_last_overlay_hotkey_key[0] = '\0';

    g_get_next_enum_calls = 0;
    g_last_get_next_enum_key[0] = '\0';
    g_last_get_next_enum_value[0] = '\0';
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->entry = gtk_entry_new();
    app->mode_indicator = gtk_label_new(">");
}

static GdkEventKey make_key(guint keyval, GdkModifierType state) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = keyval;
    event.state = state;
    return event;
}

static void seed_hotkeys(AppData *app) {
    app->hotkey_config.count = 3;
    strcpy(app->hotkey_config.bindings[0].key, "Mod4+w");
    strcpy(app->hotkey_config.bindings[0].command, "show windows");
    strcpy(app->hotkey_config.bindings[1].key, "Mod4+c");
    strcpy(app->hotkey_config.bindings[1].command, "show command");
    strcpy(app->hotkey_config.bindings[2].key, "Mod4+r");
    strcpy(app->hotkey_config.bindings[2].command, "show run");

    app->filtered_hotkeys_count = 3;
    app->filtered_hotkeys[0] = app->hotkey_config.bindings[0];
    app->filtered_hotkeys[1] = app->hotkey_config.bindings[1];
    app->filtered_hotkeys[2] = app->hotkey_config.bindings[2];
}

static void test_ctrl_e_names_tab_shows_edit_overlay_for_selected_named(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_NAMES;
    app.filtered_names_count = 2;
    app.selection.names_index = 1;
    strcpy(app.filtered_names[0].custom_name, "alpha");
    strcpy(app.filtered_names[1].custom_name, "beta");
    app.filtered_names[1].id = (Window)0xBEEF;

    GdkEventKey ev = make_key(GDK_KEY_e, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+e on Names handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+e on Names shows name-edit overlay", g_show_name_edit_calls == 1);
    ASSERT_TRUE("Ctrl+e on Names overlay targets selected index", g_last_name_edit_index == 1);
    ASSERT_TRUE("Ctrl+e on Names overlay targets selected named entry",
                strcmp(g_last_name_edit_named.custom_name, "beta") == 0 &&
                g_last_name_edit_named.id == (Window)0xBEEF);
}

static void test_ctrl_d_names_tab_deletes_custom_name(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_NAMES;
    app.names.count = 2;
    strcpy(app.names.entries[0].custom_name, "alpha");
    strcpy(app.names.entries[1].custom_name, "beta");
    app.filtered_names_count = 1;
    app.selection.names_index = 0;
    strcpy(app.filtered_names[0].custom_name, "beta");

    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+d on Names handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+d on Names removes target custom name",
                app.names.count == 1 && strcmp(app.names.entries[0].custom_name, "alpha") == 0);
    ASSERT_TRUE("Ctrl+d on Names persists names", g_save_named_windows_calls == 1);
}

static void test_ctrl_d_harpoon_tab_delete_overlay_only_for_assigned_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_HARPOON;
    app.filtered_harpoon_count = 1;
    app.selection.harpoon_index = 0;
    app.filtered_harpoon_indices[0] = 5;

    app.filtered_harpoon[0].assigned = 1;
    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled_assigned = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+d on Harpoon assigned handled", handled_assigned == TRUE);
    ASSERT_TRUE("Ctrl+d on Harpoon assigned shows delete-confirm overlay",
                g_show_harpoon_delete_calls == 1 && g_last_harpoon_delete_slot == 5);

    app.filtered_harpoon[0].assigned = 0;
    gboolean handled_unassigned = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+d on Harpoon unassigned not handled by harpoon delete path", handled_unassigned == FALSE);
    ASSERT_TRUE("Ctrl+d on Harpoon unassigned does not show overlay again", g_show_harpoon_delete_calls == 1);
}

static void test_ctrl_e_harpoon_tab_edit_overlay_only_for_assigned_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_HARPOON;
    app.filtered_harpoon_count = 1;
    app.selection.harpoon_index = 0;
    app.filtered_harpoon_indices[0] = 12;

    app.filtered_harpoon[0].assigned = 1;
    GdkEventKey ev = make_key(GDK_KEY_e, GDK_CONTROL_MASK);
    gboolean handled_assigned = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+e on Harpoon assigned handled", handled_assigned == TRUE);
    ASSERT_TRUE("Ctrl+e on Harpoon assigned shows edit overlay for selected slot",
                g_show_harpoon_edit_calls == 1 && g_last_harpoon_edit_slot == 12);

    app.filtered_harpoon[0].assigned = 0;
    gboolean handled_unassigned = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+e on Harpoon unassigned not handled by edit path", handled_unassigned == FALSE);
    ASSERT_TRUE("Ctrl+e on Harpoon unassigned does not show edit overlay again", g_show_harpoon_edit_calls == 1);
}

static void test_ctrl_t_config_tab_cycles_bool_and_saves(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_CONFIG;
    app.selection.config_index = 0;
    app.filtered_config_count = 1;
    strcpy(app.filtered_config[0].key, "close_on_focus_loss");
    strcpy(app.filtered_config[0].value, "true");
    app.filtered_config[0].type = CONFIG_TYPE_BOOL;
    app.config.close_on_focus_loss = 1;

    GdkEventKey ev = make_key(GDK_KEY_t, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+t on Config bool handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+t on Config bool flips value true->false",
                app.config.close_on_focus_loss == 0 && strcmp(app.filtered_config[0].value, "false") == 0);
    ASSERT_TRUE("Ctrl+t on Config bool saves config", g_save_config_calls == 1);
}

static void test_ctrl_t_config_tab_cycles_enum_and_saves(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_CONFIG;
    app.selection.config_index = 0;
    app.filtered_config_count = 1;
    strcpy(app.filtered_config[0].key, "digit_slot_mode");
    strcpy(app.filtered_config[0].value, "default");
    app.filtered_config[0].type = CONFIG_TYPE_ENUM;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;

    GdkEventKey ev = make_key(GDK_KEY_t, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+t on Config enum handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+t on Config enum asks next value for selected entry",
                g_get_next_enum_calls == 1 &&
                strcmp(g_last_get_next_enum_key, "digit_slot_mode") == 0 &&
                strcmp(g_last_get_next_enum_value, "default") == 0);
    ASSERT_TRUE("Ctrl+t on Config enum cycles to next value",
                app.config.digit_slot_mode == DIGIT_MODE_PER_WORKSPACE &&
                strcmp(app.filtered_config[0].value, "per-workspace") == 0);
    ASSERT_TRUE("Ctrl+t on Config enum saves config", g_save_config_calls == 1);
}

static void test_ctrl_e_config_tab_shows_edit_overlay_for_selected_entry(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_CONFIG;
    app.filtered_config_count = 2;
    app.selection.config_index = 1;
    strcpy(app.filtered_config[0].key, "close_on_focus_loss");
    strcpy(app.filtered_config[1].key, "digit_slot_mode");

    GdkEventKey ev = make_key(GDK_KEY_e, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+e on Config handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+e on Config opens config-edit overlay", g_show_overlay_calls == 1 && g_last_overlay_type == OVERLAY_CONFIG_EDIT);
    ASSERT_TRUE("Ctrl+e on Config overlay targets selected config entry", strcmp(g_last_overlay_config_key, "digit_slot_mode") == 0);
}

static void test_ctrl_a_hotkeys_tab_starts_capture_overlay(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_HOTKEYS;
    app.hotkey_capture_active = FALSE;

    GdkEventKey ev = make_key(GDK_KEY_a, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+a on Hotkeys handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+a on Hotkeys cleanup called", g_cleanup_hotkeys_calls == 1);
    ASSERT_TRUE("Ctrl+a on Hotkeys sets capture active", app.hotkey_capture_active == TRUE);
    ASSERT_TRUE("Ctrl+a on Hotkeys opens hotkey-add overlay", g_show_overlay_calls == 1 && g_last_overlay_type == OVERLAY_HOTKEY_ADD);
}

static void test_ctrl_e_hotkeys_tab_opens_edit_for_selected_binding(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_HOTKEYS;
    seed_hotkeys(&app);
    app.selection.hotkeys_index = 1;

    GdkEventKey ev = make_key(GDK_KEY_e, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+e on Hotkeys handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+e on Hotkeys opens hotkey-edit overlay", g_show_overlay_calls == 1 && g_last_overlay_type == OVERLAY_HOTKEY_EDIT);
    ASSERT_TRUE("Ctrl+e on Hotkeys overlay targets selected binding", strcmp(g_last_overlay_hotkey_key, "Mod4+c") == 0);
}

static void test_ctrl_d_hotkeys_tab_removes_binding_and_regrabs(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_HOTKEYS;
    seed_hotkeys(&app);
    app.selection.hotkeys_index = 0;

    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+d on Hotkeys handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+d on Hotkeys removes selected binding from config",
                app.hotkey_config.count == 2 && strcmp(app.hotkey_config.bindings[0].key, "Mod4+c") == 0);
    ASSERT_TRUE("Ctrl+d on Hotkeys saves config + regrabs",
                g_save_hotkey_config_calls == 1 && g_regrab_hotkeys_calls == 1);
}

static void test_ctrl_d_hotkeys_last_row_clamps_selection(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_HOTKEYS;
    seed_hotkeys(&app);
    app.selection.hotkeys_index = 2;

    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+d on Hotkeys last row handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+d on Hotkeys last row removes selected binding",
                app.hotkey_config.count == 2 && strcmp(app.hotkey_config.bindings[1].key, "Mod4+c") == 0);
    ASSERT_TRUE("Ctrl+d on Hotkeys last row clamps selection index", app.selection.hotkeys_index == 1);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Key handler tab-specific tests\n");
        printf("==============================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Key handler tab-specific tests\n");
    printf("==============================\n\n");

    test_ctrl_e_names_tab_shows_edit_overlay_for_selected_named();
    test_ctrl_d_names_tab_deletes_custom_name();
    test_ctrl_d_harpoon_tab_delete_overlay_only_for_assigned_slot();
    test_ctrl_e_harpoon_tab_edit_overlay_only_for_assigned_slot();
    test_ctrl_t_config_tab_cycles_bool_and_saves();
    test_ctrl_t_config_tab_cycles_enum_and_saves();
    test_ctrl_e_config_tab_shows_edit_overlay_for_selected_entry();
    test_ctrl_a_hotkeys_tab_starts_capture_overlay();
    test_ctrl_e_hotkeys_tab_opens_edit_for_selected_binding();
    test_ctrl_d_hotkeys_tab_removes_binding_and_regrabs();
    test_ctrl_d_hotkeys_last_row_clamps_selection();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
