#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/config_provider.h"
#include "../src/harpoon_provider.h"
#include "../src/hotkeys_provider.h"
#include "../src/key_handler.h"
#include "../src/matching_provider.h"
#include "../src/rules_provider.h"
#include "../src/projects_parse.h"
#include "../src/projects_provider.h"

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
static int g_show_name_pattern_edit_calls;
static int g_last_name_edit_index;
static MatchEntry g_last_name_edit_named;

static int g_show_name_delete_calls;
static int g_last_name_delete_manager_index;
static char g_last_matching_delete_custom_name[MAX_TITLE_LEN];

static int g_show_harpoon_delete_calls;
static int g_last_harpoon_delete_slot;

static int g_show_harpoon_edit_calls;
static int g_last_harpoon_edit_slot;

static int g_save_config_calls;
static int g_regrab_hotkeys_calls;
static int g_save_hotkey_config_calls;
static int g_update_display_calls;
static int g_save_match_entries_calls;
static int g_cleanup_hotkeys_calls;
static int g_replay_selected_rule_calls;
static int g_replay_all_rules_calls;
static int g_show_project_kill_calls;
static int g_show_project_rename_calls;
static int g_show_project_new_calls;
static char g_last_session_name[MAX_PROJECT_SESSION_NAME_LEN];
static ProjectBackend g_last_session_backend;
static char g_last_session_start_dir[1024];
static char g_last_session_initial_name[MAX_PROJECT_SESSION_NAME_LEN];
static ProjectSessionEntry g_stub_selected_session;
static gboolean g_stub_has_selected_session;
static ProjectFolder g_stub_selected_folder;
static gboolean g_stub_has_selected_folder;

#define TEST_HARPOON_TAB  ((TabMode)(TAB_COUNT + 1))
#define TEST_PROJECTS_TAB ((TabMode)(TAB_COUNT + 2))
#define TEST_MATCHING_TAB    ((TabMode)(TAB_COUNT + 3))
#define TEST_CONFIG_TAB   ((TabMode)(TAB_COUNT + 4))
#define TEST_HOTKEYS_TAB  ((TabMode)(TAB_COUNT + 5))
#define TEST_RULES_TAB    ((TabMode)(TAB_COUNT + 6))
#define TEST_APPS_TAB     ((TabMode)(TAB_COUNT + 7))

static int g_show_overlay_calls;
static OverlayType g_last_overlay_type;
static void *g_last_overlay_data;
static char g_last_overlay_config_key[64];
static char g_last_overlay_hotkey_key[64];

static int g_get_next_enum_calls;
static char g_last_get_next_enum_key[64];
static char g_last_get_next_enum_value[64];
static int g_last_provider_tab_lookup = -1;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

int has_match(const char *pattern, const char *text) {
    return !pattern || pattern[0] == '\0' || (text && strstr(text, pattern) != NULL);
}

void cofi_init_provider_defaults(CofiTabProvider *provider) {
    if (provider) memset(provider, 0, sizeof(*provider));
}

int cofi_register_tab_provider(const CofiTabProvider *provider) {
    (void)provider;
    return 0;
}

/* --- Stubs required by key_handler.c --- */
gboolean is_overlay_active(AppData *app) { (void)app; return FALSE; }
gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event) { (void)app; (void)event; return FALSE; }
gboolean handle_command_key(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }
void command_update_candidates(CommandMode *cmd, const char *text) { (void)cmd; (void)text; }
gboolean handle_tab_switching(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }
void switch_to_tab(AppData *app, TabMode target_tab) { app->current_tab = target_tab; }
void surface_tab(AppData *app, TabMode tab) { if (app) app->current_tab = tab; }
void sinks_switch_selected(AppData *app) { (void)app; }
void sinks_filter(AppData *app, const char *filter) { (void)app; (void)filter; }
gboolean sinks_switch_name(AppData *app, const char *sink_name) { (void)app; (void)sink_name; return TRUE; }
gboolean sinks_assign_selected_slot(AppData *app, char slot_key) { (void)app; (void)slot_key; return FALSE; }
gboolean sinks_switch_slot(AppData *app, char slot_key) { (void)app; (void)slot_key; return FALSE; }
gboolean proc_signal_selected_with_modifiers(AppData *app, guint state) { (void)app; (void)state; return TRUE; }
void proc_filter(AppData *app, const char *filter) { (void)app; (void)filter; }

void enter_command_mode(AppData *app) { (void)app; }
void exit_command_mode(AppData *app) { if (app) { app->command_mode.state = CMD_MODE_NORMAL; app->active_prefix_claim = '\0'; } }
void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) { (void)app; (void)provider; }
void cofi_exit_modal(AppData *app) { (void)app; }
gboolean cofi_handle_modal_key(AppData *app, GdkEventKey *event) { (void)app; (void)event; return FALSE; }
const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) { (void)prefix; return NULL; }
const CofiTabProvider *cofi_get_provider_for_tab_prefix(char prefix) { (void)prefix; return NULL; }
const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    static CofiTabProvider matching_provider;
    static CofiTabProvider config_provider;
    static CofiTabProvider hotkeys_provider;
    static CofiTabProvider rules_provider;
    static CofiTabProvider projects_provider;
    static CofiTabProvider harpoon_provider;
    memset(&matching_provider, 0, sizeof(matching_provider));
    memset(&config_provider, 0, sizeof(config_provider));
    memset(&hotkeys_provider, 0, sizeof(hotkeys_provider));
    memset(&rules_provider, 0, sizeof(rules_provider));
    memset(&projects_provider, 0, sizeof(projects_provider));
    memset(&harpoon_provider, 0, sizeof(harpoon_provider));
    g_last_provider_tab_lookup = tab_mode;
    matching_provider.tab_mode = TEST_MATCHING_TAB;
    matching_provider.handle_key = handle_matching_tab_keys;
    config_provider.tab_mode = TEST_CONFIG_TAB;
    config_provider.handle_key = handle_config_tab_keys;
    hotkeys_provider.tab_mode = TEST_HOTKEYS_TAB;
    hotkeys_provider.handle_key = handle_hotkeys_tab_keys;
    rules_provider.tab_mode = TEST_RULES_TAB;
    rules_provider.handle_key = handle_rules_tab_keys;
    projects_provider.tab_mode = TEST_PROJECTS_TAB;
    projects_provider.handle_key = handle_projects_tab_keys;
    harpoon_provider.tab_mode = TEST_HARPOON_TAB;
    harpoon_provider.handle_key = handle_harpoon_tab_keys;

    if (tab_mode == TEST_PROJECTS_TAB) return &projects_provider;
    if (tab_mode == TEST_MATCHING_TAB) return &matching_provider;
    if (tab_mode == TEST_CONFIG_TAB) return &config_provider;
    if (tab_mode == TEST_HOTKEYS_TAB) return &hotkeys_provider;
    if (tab_mode == TEST_RULES_TAB) return &rules_provider;
    if (tab_mode == TEST_HARPOON_TAB) return &harpoon_provider;

    switch ((TabMode)tab_mode) {
        default: return NULL;
    }
}
int cofi_get_provider_id_for_tab(int tab_mode) { (void)tab_mode; return -1; }
int cofi_filtered_to_raw(int provider_id, int filtered_idx) { (void)provider_id; return filtered_idx; }
TabMode apps_tab_mode(void) { return TEST_APPS_TAB; }
const CofiTabProvider *cofi_get_provider(int provider_id) {
    static CofiTabProvider matching_provider;
    static CofiTabProvider config_provider;
    static CofiTabProvider hotkeys_provider;
    static CofiTabProvider rules_provider;
    static CofiTabProvider harpoon_provider;
    (void)provider_id;
    memset(&matching_provider, 0, sizeof(matching_provider));
    memset(&config_provider, 0, sizeof(config_provider));
    memset(&hotkeys_provider, 0, sizeof(hotkeys_provider));
    memset(&rules_provider, 0, sizeof(rules_provider));
    memset(&harpoon_provider, 0, sizeof(harpoon_provider));
    matching_provider.tab_mode = TEST_MATCHING_TAB;
    config_provider.tab_mode = TEST_CONFIG_TAB;
    hotkeys_provider.tab_mode = TEST_HOTKEYS_TAB;
    rules_provider.tab_mode = TEST_RULES_TAB;
    harpoon_provider.tab_mode = TEST_HARPOON_TAB;
    if (g_last_provider_tab_lookup == TEST_HARPOON_TAB)
        return &harpoon_provider;
    if (g_last_provider_tab_lookup == TEST_CONFIG_TAB)
        return &config_provider;
    if (g_last_provider_tab_lookup == TEST_HOTKEYS_TAB)
        return &hotkeys_provider;
    return g_last_provider_tab_lookup == TEST_RULES_TAB ? &rules_provider : &matching_provider;
}

WindowInfo *get_selected_window(AppData *app) { (void)app; return NULL; }
void move_selection_up(AppData *app) { (void)app; }
void move_selection_down(AppData *app) { (void)app; }
int get_selected_index(AppData *app) { (void)app; return 0; }
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
    g_last_name_edit_index = app ? app->selection.provider_index : -1;
    if (app && app->selection.provider_index >= 0 && app->selection.provider_index < app->filtered_matching_count) {
        g_last_name_edit_named = app->filtered_matching[app->selection.provider_index];
    } else {
        memset(&g_last_name_edit_named, 0, sizeof(g_last_name_edit_named));
    }
}

void show_name_pattern_edit_overlay(AppData *app) {
    (void)app;
    g_show_name_pattern_edit_calls++;
}

void show_name_delete_overlay(AppData *app, const char *custom_name, int manager_index) {
    (void)app;
    g_show_name_delete_calls++;
    g_last_name_delete_manager_index = manager_index;
    strncpy(g_last_matching_delete_custom_name,
            custom_name ? custom_name : "",
            sizeof(g_last_matching_delete_custom_name) - 1);
    g_last_matching_delete_custom_name[sizeof(g_last_matching_delete_custom_name) - 1] = '\0';
}

int match_entry_find_index_by_window(const MatchEntryManager *manager, Window id) {
    if (!manager) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].bound_x11_id == id) {
            return i;
        }
    }
    return -1;
}

int match_entry_find_index_by_custom_name(const MatchEntryManager *manager, const char *custom_name) {
    if (!manager || !custom_name) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->entries[i].custom_name, custom_name) == 0) {
            return i;
        }
    }
    return -1;
}

void match_entry_delete_custom_name(MatchEntryManager *manager, int index) {
    if (!manager || index < 0 || index >= manager->count) return;
    for (int i = index; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    manager->count--;
}

void save_match_entries(const MatchEntryManager *manager) { (void)manager; g_save_match_entries_calls++; }

void filter_matching(AppData *app, const char *filter) {
    (void)filter;
    app->filtered_matching_count = app->matching.count;
    for (int i = 0; i < app->matching.count; i++) {
        app->filtered_matching[i] = app->matching.entries[i];
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

void build_config_entries(const CofiConfig *config, ConfigEntry *entries, int *count) {
    *count = 2;
    strcpy(entries[0].key, "close_on_focus_loss");
    strcpy(entries[0].value, config->close_on_focus_loss ? "true" : "false");
    entries[0].type = CONFIG_TYPE_BOOL;

    strcpy(entries[1].key, "digit_slot_mode");
    if (config->digit_slot_mode == DIGIT_MODE_PER_WORKSPACE) {
        strcpy(entries[1].value, "per-workspace");
    } else if (config->digit_slot_mode == DIGIT_MODE_WORKSPACES) {
        strcpy(entries[1].value, "workspaces");
    } else {
        strcpy(entries[1].value, "default");
    }
    entries[1].type = CONFIG_TYPE_ENUM;
}

void show_overlay(AppData *app, OverlayType type, void *data) {
    g_show_overlay_calls++;
    g_last_overlay_type = type;
    g_last_overlay_data = data;

    g_last_overlay_config_key[0] = '\0';
    g_last_overlay_hotkey_key[0] = '\0';

    if (!app) return;

    if (type == OVERLAY_CONFIG_EDIT &&
        app->selection.provider_index >= 0 && app->selection.provider_index < app->filtered_config_count) {
        strncpy(g_last_overlay_config_key,
                app->filtered_config[app->selection.provider_index].key,
                sizeof(g_last_overlay_config_key) - 1);
    }

    if (type == OVERLAY_HOTKEY_EDIT &&
        app->selection.provider_index >= 0 && app->selection.provider_index < app->filtered_hotkeys_count) {
        strncpy(g_last_overlay_hotkey_key,
                app->filtered_hotkeys[app->selection.provider_index].key,
                sizeof(g_last_overlay_hotkey_key) - 1);
    }
}

void cleanup_hotkeys(AppData *app) { (void)app; g_cleanup_hotkeys_calls++; }

int parse_hotkey_command(const char *args, char *key, size_t key_size,
                         char *cmd, size_t cmd_size) {
    (void)args;
    if (key && key_size > 0) key[0] = '\0';
    if (cmd && cmd_size > 0) cmd[0] = '\0';
    return 0;
}

int add_hotkey_binding(HotkeyConfig *config, const char *key, const char *command) {
    (void)config;
    (void)key;
    (void)command;
    return 1;
}

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
int replay_all_rules_against_open_windows(AppData *app) { (void)app; g_replay_all_rules_calls++; return 0; }
gboolean replay_selected_filtered_rule(AppData *app) { (void)app; g_replay_selected_rule_calls++; return TRUE; }

void filter_windows(AppData *app, const char *query) { (void)app; (void)query; }
void filter_workspaces(AppData *app, const char *query) { (void)app; (void)query; }
void filter_apps(AppData *app, const char *query) { (void)app; (void)query; }
void reset_selection(AppData *app) { (void)app; }
void preserve_selection(AppData *app) { (void)app; }
void restore_selection(AppData *app) { (void)app; }
void apps_launch(const AppEntry *entry) { (void)entry; }
ProjectSessionEntry *projects_selected_session(AppData *app) {
    (void)app;
    return g_stub_has_selected_session ? &g_stub_selected_session : NULL;
}

ProjectFolder *projects_selected_folder(AppData *app) {
    (void)app;
    return g_stub_has_selected_folder ? &g_stub_selected_folder : NULL;
}

void show_project_kill_overlay(AppData *app, const char *session_name, ProjectBackend backend) {
    (void)app;
    g_show_project_kill_calls++;
    g_last_session_backend = backend;
    strncpy(g_last_session_name, session_name ? session_name : "",
            sizeof(g_last_session_name) - 1);
    g_last_session_name[sizeof(g_last_session_name) - 1] = '\0';
}

void show_project_rename_overlay(AppData *app, const char *session_name) {
    (void)app;
    g_show_project_rename_calls++;
    strncpy(g_last_session_name, session_name ? session_name : "",
            sizeof(g_last_session_name) - 1);
    g_last_session_name[sizeof(g_last_session_name) - 1] = '\0';
}

void show_project_new_overlay(AppData *app,
                              ProjectBackend backend,
                              const char *start_dir,
                              const char *initial_name) {
    (void)app;
    g_show_project_new_calls++;
    g_last_session_backend = backend;
    strncpy(g_last_session_start_dir, start_dir ? start_dir : "",
            sizeof(g_last_session_start_dir) - 1);
    g_last_session_start_dir[sizeof(g_last_session_start_dir) - 1] = '\0';
    strncpy(g_last_session_initial_name, initial_name ? initial_name : "",
            sizeof(g_last_session_initial_name) - 1);
    g_last_session_initial_name[sizeof(g_last_session_initial_name) - 1] = '\0';
}

gboolean handle_projects_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TEST_PROJECTS_TAB) {
        return FALSE;
    }

    gboolean ctrl_n =
        (event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N);
    if (event->keyval == GDK_KEY_Insert || event->keyval == GDK_KEY_KP_Insert || ctrl_n) {
        ProjectBackend backend =
            (event->state & GDK_SHIFT_MASK) ? PROJECT_BACKEND_ZELLIJ : PROJECT_BACKEND_TMUX;
        ProjectSessionEntry *session = projects_selected_session(app);
        if (!(event->state & GDK_SHIFT_MASK) && session &&
            session->backend == PROJECT_BACKEND_ZELLIJ) {
            backend = PROJECT_BACKEND_ZELLIJ;
        }
        ProjectFolder *folder = projects_selected_folder(app);
        if (!folder) {
            show_project_new_overlay(app, backend, "", "");
            return TRUE;
        }
        gchar *session_name = projects_build_folder_session_name(folder->path);
        show_project_new_overlay(app, backend, folder->path, session_name);
        g_free(session_name);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
        ProjectSessionEntry *session = projects_selected_session(app);
        if (!session) {
            return FALSE;
        }
        show_project_kill_overlay(app, session->name, session->backend);
        return TRUE;
    }

    gboolean ctrl_r =
        (event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_r || event->keyval == GDK_KEY_R);
    if (event->keyval == GDK_KEY_F2 || ctrl_r) {
        ProjectSessionEntry *session = projects_selected_session(app);
        if (!session || session->backend != PROJECT_BACKEND_TMUX) {
            return FALSE;
        }
        show_project_rename_overlay(app, session->name);
        return TRUE;
    }

    return FALSE;
}

#include "../src/key_handler.c"

static void reset_captures(void) {
    g_show_name_edit_calls = 0;
    g_show_name_pattern_edit_calls = 0;
    g_last_name_edit_index = -1;
    memset(&g_last_name_edit_named, 0, sizeof(g_last_name_edit_named));

    g_show_name_delete_calls = 0;
    g_last_name_delete_manager_index = -1;
    g_last_matching_delete_custom_name[0] = '\0';

    g_show_harpoon_delete_calls = 0;
    g_last_harpoon_delete_slot = -1;
    g_show_harpoon_edit_calls = 0;
    g_last_harpoon_edit_slot = -1;

    g_save_config_calls = 0;
    g_regrab_hotkeys_calls = 0;
    g_save_hotkey_config_calls = 0;
    g_update_display_calls = 0;
    g_save_match_entries_calls = 0;
    g_cleanup_hotkeys_calls = 0;
    g_replay_selected_rule_calls = 0;
    g_replay_all_rules_calls = 0;
    g_show_project_kill_calls = 0;
    g_show_project_rename_calls = 0;
    g_show_project_new_calls = 0;
    g_last_session_name[0] = '\0';
    g_last_session_start_dir[0] = '\0';
    g_last_session_initial_name[0] = '\0';
    g_last_session_backend = PROJECT_BACKEND_TMUX;
    memset(&g_stub_selected_session, 0, sizeof(g_stub_selected_session));
    g_stub_has_selected_session = FALSE;
    memset(&g_stub_selected_folder, 0, sizeof(g_stub_selected_folder));
    g_stub_has_selected_folder = FALSE;

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
    app->filtered_hotkeys_indices[0] = 0;
    app->filtered_hotkeys_indices[1] = 1;
    app->filtered_hotkeys_indices[2] = 2;
}

static void test_ctrl_e_names_tab_shows_edit_overlay_for_selected_named(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_MATCHING_TAB;
    app.filtered_matching_count = 2;
    app.selection.provider_index = 1;
    strcpy(app.filtered_matching[0].custom_name, "alpha");
    strcpy(app.filtered_matching[1].custom_name, "beta");
    app.filtered_matching[1].bound_x11_id = (Window)0xBEEF;

    GdkEventKey ev = make_key(GDK_KEY_e, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+e on Names handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+e on Names shows name-edit overlay", g_show_name_edit_calls == 1);
    ASSERT_TRUE("Ctrl+e on Names overlay targets selected index", g_last_name_edit_index == 1);
    ASSERT_TRUE("Ctrl+e on Names overlay targets selected named entry",
                strcmp(g_last_name_edit_named.custom_name, "beta") == 0 &&
                g_last_name_edit_named.bound_x11_id == (Window)0xBEEF);
}

static void test_ctrl_d_names_tab_shows_delete_confirm_overlay(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_MATCHING_TAB;
    app.matching.count = 2;
    strcpy(app.matching.entries[0].custom_name, "alpha");
    strcpy(app.matching.entries[1].custom_name, "beta");
    app.filtered_matching_count = 1;
    app.selection.provider_index = 0;
    strcpy(app.filtered_matching[0].custom_name, "beta");

    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+d on Names handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+d on Names shows delete-confirm overlay",
                g_show_name_delete_calls == 1 &&
                strcmp(g_last_matching_delete_custom_name, "beta") == 0 &&
                g_last_name_delete_manager_index == 1);
    ASSERT_TRUE("Ctrl+d on Names does not delete immediately",
                app.matching.count == 2 && g_save_match_entries_calls == 0);
}

static void test_ctrl_p_matching_tab_shows_pattern_overlay_for_selected_entry(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_MATCHING_TAB;
    app.filtered_matching_count = 1;
    app.selection.provider_index = 0;
    strcpy(app.filtered_matching[0].custom_name, "beta");

    GdkEventKey ev = make_key(GDK_KEY_p, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+p on Matching handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+p on Matching shows pattern edit overlay",
                g_show_name_pattern_edit_calls == 1);
}

static void test_ctrl_d_names_tab_shows_overlay_even_without_resolved_manager_index(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_MATCHING_TAB;
    app.matching.count = 0;
    app.filtered_matching_count = 1;
    app.selection.provider_index = 0;
    app.filtered_matching[0].bound_x11_id = (Window)0xDEAD;
    strcpy(app.filtered_matching[0].custom_name, "orphan");

    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+d on unresolved Names row still handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+d on unresolved Names row still shows overlay",
                g_show_name_delete_calls == 1 &&
                strcmp(g_last_matching_delete_custom_name, "orphan") == 0 &&
                g_last_name_delete_manager_index == -1);
}

static void test_ctrl_d_harpoon_tab_delete_overlay_only_for_assigned_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_HARPOON_TAB;
    app.filtered_harpoon_count = 1;
    app.selection.provider_index = 0;
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

    app.current_tab = TEST_HARPOON_TAB;
    app.filtered_harpoon_count = 1;
    app.selection.provider_index = 0;
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

    app.current_tab = TEST_CONFIG_TAB;
    app.selection.provider_index = 0;
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

    app.current_tab = TEST_CONFIG_TAB;
    app.selection.provider_index = 0;
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
                strcmp(app.filtered_config[app.selection.provider_index].value,
                       "per-workspace") == 0);
    ASSERT_TRUE("Ctrl+t on Config enum saves config", g_save_config_calls == 1);
}

static void test_ctrl_e_config_tab_shows_edit_overlay_for_value_entry(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_CONFIG_TAB;
    app.filtered_config_count = 2;
    app.selection.provider_index = 1;
    strcpy(app.filtered_config[0].key, "close_on_focus_loss");
    app.filtered_config[0].type = CONFIG_TYPE_BOOL;
    strcpy(app.filtered_config[1].key, "tile_columns");
    app.filtered_config[1].type = CONFIG_TYPE_INT;

    GdkEventKey ev = make_key(GDK_KEY_e, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+e on Config handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+e on Config opens config-edit overlay", g_show_overlay_calls == 1 && g_last_overlay_type == OVERLAY_CONFIG_EDIT);
    ASSERT_TRUE("Ctrl+e on Config overlay targets selected config entry", strcmp(g_last_overlay_config_key, "tile_columns") == 0);
}

static void test_ctrl_e_config_tab_ignores_enum_entry(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_CONFIG_TAB;
    app.filtered_config_count = 1;
    app.selection.provider_index = 0;
    strcpy(app.filtered_config[0].key, "digit_slot_mode");
    strcpy(app.filtered_config[0].value, "default");
    app.filtered_config[0].type = CONFIG_TYPE_ENUM;

    GdkEventKey ev = make_key(GDK_KEY_e, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+e on Config enum not handled", handled == FALSE);
    ASSERT_TRUE("Ctrl+e on Config enum does not open overlay", g_show_overlay_calls == 0);
}

static void test_ctrl_a_hotkeys_tab_starts_capture_overlay(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_HOTKEYS_TAB;
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

    app.current_tab = TEST_HOTKEYS_TAB;
    seed_hotkeys(&app);
    app.selection.provider_index = 1;

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

    app.current_tab = TEST_HOTKEYS_TAB;
    seed_hotkeys(&app);
    app.selection.provider_index = 0;

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

    app.current_tab = TEST_HOTKEYS_TAB;
    seed_hotkeys(&app);
    app.selection.provider_index = 2;

    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+d on Hotkeys last row handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+d on Hotkeys last row removes selected binding",
                app.hotkey_config.count == 2 && strcmp(app.hotkey_config.bindings[1].key, "Mod4+c") == 0);
    ASSERT_TRUE("Ctrl+d on Hotkeys last row clamps selection index", app.selection.provider_index == 1);
}

static void test_rules_tab_shortcuts_crud_and_replay(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_RULES_TAB;
    app.filtered_rules_count = 2;
    app.selection.provider_index = 1;
    app.filtered_rule_indices[0] = 3;
    app.filtered_rule_indices[1] = 7;

    GdkEventKey add_ev = make_key(GDK_KEY_a, GDK_CONTROL_MASK);
    gboolean add_handled = on_key_press(NULL, &add_ev, &app);
    ASSERT_TRUE("Ctrl+a on Rules handled", add_handled == TRUE);
    ASSERT_TRUE("Ctrl+a on Rules opens add overlay",
                g_show_overlay_calls == 1 && g_last_overlay_type == OVERLAY_RULE_ADD);

    GdkEventKey edit_ev = make_key(GDK_KEY_e, GDK_CONTROL_MASK);
    gboolean edit_handled = on_key_press(NULL, &edit_ev, &app);
    ASSERT_TRUE("Ctrl+e on Rules handled", edit_handled == TRUE);
    ASSERT_TRUE("Ctrl+e on Rules opens edit overlay",
                g_show_overlay_calls == 2 && g_last_overlay_type == OVERLAY_RULE_EDIT);

    GdkEventKey del_ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean del_handled = on_key_press(NULL, &del_ev, &app);
    ASSERT_TRUE("Ctrl+d on Rules handled", del_handled == TRUE);
    ASSERT_TRUE("Ctrl+d on Rules opens delete overlay",
                g_show_overlay_calls == 3 && g_last_overlay_type == OVERLAY_RULE_DELETE);
    ASSERT_TRUE("Ctrl+d on Rules stores selected rule index", app.rules_delete.rule_index == 7);

    GdkEventKey replay_sel = make_key(GDK_KEY_x, GDK_CONTROL_MASK);
    gboolean replay_sel_handled = on_key_press(NULL, &replay_sel, &app);
    ASSERT_TRUE("Ctrl+x on Rules handled", replay_sel_handled == TRUE);
    ASSERT_TRUE("Ctrl+x on Rules replays selected", g_replay_selected_rule_calls == 1);

    GdkEventKey replay_all = make_key(GDK_KEY_X, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    gboolean replay_all_handled = on_key_press(NULL, &replay_all, &app);
    ASSERT_TRUE("Ctrl+Shift+x on Rules handled", replay_all_handled == TRUE);
    ASSERT_TRUE("Ctrl+Shift+x on Rules replays all", g_replay_all_rules_calls == 1);
}

static void test_projects_tab_shortcuts_open_session_overlays(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_PROJECTS_TAB;
    g_stub_has_selected_session = TRUE;
    g_stub_selected_session.backend = PROJECT_BACKEND_TMUX;
    strncpy(g_stub_selected_session.name, "work:api session",
            sizeof(g_stub_selected_session.name) - 1);

    GdkEventKey del_ev = make_key(GDK_KEY_Delete, 0);
    gboolean del_handled = on_key_press(NULL, &del_ev, &app);
    ASSERT_TRUE("Delete on tmux session handled", del_handled == TRUE);
    ASSERT_TRUE("Delete on tmux session opens kill overlay",
                g_show_project_kill_calls == 1 &&
                g_last_session_backend == PROJECT_BACKEND_TMUX &&
                strcmp(g_last_session_name, "work:api session") == 0);

    GdkEventKey f2_ev = make_key(GDK_KEY_F2, 0);
    gboolean f2_handled = on_key_press(NULL, &f2_ev, &app);
    ASSERT_TRUE("F2 on tmux session handled", f2_handled == TRUE);
    ASSERT_TRUE("F2 on tmux session opens rename overlay",
                g_show_project_rename_calls == 1 &&
                strcmp(g_last_session_name, "work:api session") == 0);

    GdkEventKey ctrl_r_ev = make_key(GDK_KEY_r, GDK_CONTROL_MASK);
    gboolean ctrl_r_handled = on_key_press(NULL, &ctrl_r_ev, &app);
    ASSERT_TRUE("Ctrl+r on tmux session handled", ctrl_r_handled == TRUE);
    ASSERT_TRUE("Ctrl+r on tmux session opens rename overlay",
                g_show_project_rename_calls == 2 &&
                strcmp(g_last_session_name, "work:api session") == 0);

    GdkEventKey ins_ev = make_key(GDK_KEY_Insert, 0);
    gboolean ins_handled = on_key_press(NULL, &ins_ev, &app);
    ASSERT_TRUE("Insert on Projects opens new-session overlay", ins_handled == TRUE);
    ASSERT_TRUE("Insert on Projects opens exactly one new-session overlay",
                g_show_project_new_calls == 1 &&
                g_last_session_backend == PROJECT_BACKEND_TMUX &&
                strcmp(g_last_session_start_dir, "") == 0);

    GdkEventKey ctrl_n_ev = make_key(GDK_KEY_n, GDK_CONTROL_MASK);
    gboolean ctrl_n_handled = on_key_press(NULL, &ctrl_n_ev, &app);
    ASSERT_TRUE("Ctrl+n on Projects opens new-session overlay", ctrl_n_handled == TRUE);
    ASSERT_TRUE("Ctrl+n on Projects behaves like Insert",
                g_show_project_new_calls == 2 &&
                g_last_session_backend == PROJECT_BACKEND_TMUX &&
                strcmp(g_last_session_start_dir, "") == 0);

    GdkEventKey shift_ins_ev = make_key(GDK_KEY_Insert, GDK_SHIFT_MASK);
    gboolean shift_ins_handled = on_key_press(NULL, &shift_ins_ev, &app);
    ASSERT_TRUE("Shift+Insert on Projects opens new-session overlay", shift_ins_handled == TRUE);
    ASSERT_TRUE("Shift+Insert preselects zellij",
                g_show_project_new_calls == 3 &&
                g_last_session_backend == PROJECT_BACKEND_ZELLIJ);
}

static void test_projects_tab_ctrl_shift_n_is_not_new_session_shortcut(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_PROJECTS_TAB;
    g_stub_has_selected_session = TRUE;
    g_stub_selected_session.backend = PROJECT_BACKEND_TMUX;
    strncpy(g_stub_selected_session.name, "work",
            sizeof(g_stub_selected_session.name) - 1);

    GdkEventKey ev = make_key(GDK_KEY_n, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+Shift+n on Projects is left for slot assignment", handled == FALSE);
    ASSERT_TRUE("Ctrl+Shift+n on Projects does not open new-session overlay",
                g_show_project_new_calls == 0);
}

static void test_projects_tab_ctrl_shift_r_is_not_rename_shortcut(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_PROJECTS_TAB;
    g_stub_has_selected_session = TRUE;
    g_stub_selected_session.backend = PROJECT_BACKEND_TMUX;
    strncpy(g_stub_selected_session.name, "work",
            sizeof(g_stub_selected_session.name) - 1);

    GdkEventKey ev = make_key(GDK_KEY_r, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+Shift+r on Projects is left for slot assignment", handled == FALSE);
    ASSERT_TRUE("Ctrl+Shift+r on Projects does not open rename overlay",
                g_show_project_rename_calls == 0);
}

static void test_projects_tab_delete_on_zellij_session_opens_kill_overlay(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_PROJECTS_TAB;
    g_stub_has_selected_session = TRUE;
    g_stub_selected_session.backend = PROJECT_BACKEND_ZELLIJ;
    strncpy(g_stub_selected_session.name, "zj work",
            sizeof(g_stub_selected_session.name) - 1);

    GdkEventKey del_ev = make_key(GDK_KEY_Delete, 0);
    gboolean del_handled = on_key_press(NULL, &del_ev, &app);
    ASSERT_TRUE("Delete on zellij session handled", del_handled == TRUE);
    ASSERT_TRUE("Delete on zellij session opens kill overlay",
                g_show_project_kill_calls == 1 &&
                g_last_session_backend == PROJECT_BACKEND_ZELLIJ &&
                strcmp(g_last_session_name, "zj work") == 0);

    GdkEventKey ins_ev = make_key(GDK_KEY_Insert, 0);
    gboolean ins_handled = on_key_press(NULL, &ins_ev, &app);
    ASSERT_TRUE("Insert on zellij session handled", ins_handled == TRUE);
    ASSERT_TRUE("Insert on zellij session preselects zellij",
                g_show_project_new_calls == 1 &&
                g_last_session_backend == PROJECT_BACKEND_ZELLIJ);
}

static void test_projects_tab_rename_ignores_zellij_session_row(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_PROJECTS_TAB;
    g_stub_has_selected_session = TRUE;
    g_stub_selected_session.backend = PROJECT_BACKEND_ZELLIJ;
    strncpy(g_stub_selected_session.name, "zj work",
            sizeof(g_stub_selected_session.name) - 1);

    GdkEventKey f2_ev = make_key(GDK_KEY_F2, 0);
    gboolean f2_handled = on_key_press(NULL, &f2_ev, &app);
    ASSERT_TRUE("F2 on zellij session not handled", f2_handled == FALSE);
    ASSERT_TRUE("F2 on zellij session does not open rename overlay",
                g_show_project_rename_calls == 0);
}

static void test_projects_tab_delete_and_rename_ignore_folder_rows(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_PROJECTS_TAB;

    GdkEventKey del_ev = make_key(GDK_KEY_Delete, 0);
    GdkEventKey f2_ev = make_key(GDK_KEY_F2, 0);
    gboolean del_handled = on_key_press(NULL, &del_ev, &app);
    gboolean f2_handled = on_key_press(NULL, &f2_ev, &app);

    ASSERT_TRUE("Delete on projects folder/error row not handled", del_handled == FALSE);
    ASSERT_TRUE("F2 on projects folder/error row not handled", f2_handled == FALSE);
    ASSERT_TRUE("Projects folder/error row does not open kill/rename overlay",
                g_show_project_kill_calls == 0 && g_show_project_rename_calls == 0);
}

static void test_projects_tab_insert_on_folder_uses_folder_dir(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_PROJECTS_TAB;
    g_stub_has_selected_folder = TRUE;
    g_stub_selected_folder.path = "/home/user/Projects/cofi";
    g_stub_selected_folder.label = "cofi";

    GdkEventKey ins_ev = make_key(GDK_KEY_Insert, 0);
    gboolean ins_handled = on_key_press(NULL, &ins_ev, &app);
    ASSERT_TRUE("Insert on folder opens new-session overlay", ins_handled == TRUE);
    ASSERT_TRUE("Insert on folder uses tmux and folder path",
                g_show_project_new_calls == 1 &&
                g_last_session_backend == PROJECT_BACKEND_TMUX &&
                strcmp(g_last_session_start_dir, "/home/user/Projects/cofi") == 0 &&
                strcmp(g_last_session_initial_name, "cofi") == 0);

    GdkEventKey shift_ins_ev = make_key(GDK_KEY_Insert, GDK_SHIFT_MASK);
    gboolean shift_ins_handled = on_key_press(NULL, &shift_ins_ev, &app);
    ASSERT_TRUE("Shift+Insert on folder opens new-session overlay", shift_ins_handled == TRUE);
    ASSERT_TRUE("Shift+Insert on folder preselects zellij and folder path",
                g_show_project_new_calls == 2 &&
                g_last_session_backend == PROJECT_BACKEND_ZELLIJ &&
                strcmp(g_last_session_start_dir, "/home/user/Projects/cofi") == 0 &&
                strcmp(g_last_session_initial_name, "cofi") == 0);
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
    test_ctrl_d_names_tab_shows_delete_confirm_overlay();
    test_ctrl_p_matching_tab_shows_pattern_overlay_for_selected_entry();
    test_ctrl_d_names_tab_shows_overlay_even_without_resolved_manager_index();
    test_ctrl_d_harpoon_tab_delete_overlay_only_for_assigned_slot();
    test_ctrl_e_harpoon_tab_edit_overlay_only_for_assigned_slot();
    test_ctrl_t_config_tab_cycles_bool_and_saves();
    test_ctrl_t_config_tab_cycles_enum_and_saves();
    test_ctrl_e_config_tab_shows_edit_overlay_for_value_entry();
    test_ctrl_e_config_tab_ignores_enum_entry();
    test_ctrl_a_hotkeys_tab_starts_capture_overlay();
    test_ctrl_e_hotkeys_tab_opens_edit_for_selected_binding();
    test_ctrl_d_hotkeys_tab_removes_binding_and_regrabs();
    test_ctrl_d_hotkeys_last_row_clamps_selection();
    test_rules_tab_shortcuts_crud_and_replay();
    test_projects_tab_shortcuts_open_session_overlays();
    test_projects_tab_ctrl_shift_n_is_not_new_session_shortcut();
    test_projects_tab_ctrl_shift_r_is_not_rename_shortcut();
    test_projects_tab_delete_on_zellij_session_opens_kill_overlay();
    test_projects_tab_rename_ignores_zellij_session_row();
    test_projects_tab_delete_and_rename_ignore_folder_rows();
    test_projects_tab_insert_on_folder_uses_folder_dir();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
