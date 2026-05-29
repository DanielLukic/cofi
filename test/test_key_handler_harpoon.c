#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"
#include "ui/key_handler.h"
#include "core/utils/constants.h"

/*
 * Testability strategy:
 * Include key_handler.c directly. External modules are stubbed with
 * argument-capturing behavior; slot mutations asserted against AppData state.
 */

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_hide_calls;
static int g_activate_calls;
static Window g_last_activate_window;
static int g_switch_calls;
static int g_last_switch_desktop;
static int g_assign_workspace_slots_calls;
static int g_save_config_calls;
static int g_save_harpoon_calls;
static int g_update_display_calls;
static int g_workspace_switch_state;
static int g_workspace_count = 9;

static int g_highlight_calls;
static Window g_last_highlight_window;
static int g_get_workspace_slot_calls;
static int g_last_workspace_slot_query;
static int g_sinks_assign_calls;
static int g_sinks_switch_slot_calls;
static char g_last_sinks_slot;
static int g_provider_handle_key_calls;
static int g_ctrl_n_recall_calls;
static const CofiTabProvider *g_provider_for_tab;
static CofiTabProvider g_sinks_provider;
static CofiTabProvider g_ctrl_n_provider;
static int g_matching_run_gc_calls;
static int g_delete_by_match_id_calls;
static int g_last_deleted_match_id;

#define TEST_WORKSPACES_TAB ((TabMode)(TAB_COUNT + 1))
#define TEST_HARPOON_TAB    ((TabMode)(TAB_COUNT + 2))
#define TEST_SINKS_TAB      ((TabMode)(TAB_COUNT + 3))
#define TEST_APPS_TAB       ((TabMode)(TAB_COUNT + 4))

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

/* --- Stubs required by key_handler.c --- */
gboolean is_overlay_active(AppData *app) { (void)app; return FALSE; }
gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event) { (void)app; (void)event; return FALSE; }
gboolean handle_command_key(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }
void command_update_candidates(CommandMode *cmd, const char *text) { (void)cmd; (void)text; }
gboolean handle_tab_switching(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }
void switch_to_tab(AppData *app, TabMode target_tab) { app->current_tab = target_tab; }
void sinks_switch_selected(AppData *app) { (void)app; }
void sinks_filter(AppData *app, const char *filter) { (void)app; (void)filter; }
gboolean sinks_switch_name(AppData *app, const char *sink_name) { (void)app; (void)sink_name; return TRUE; }
gboolean sinks_assign_selected_slot(AppData *app, char slot_key) {
    (void)app;
    g_sinks_assign_calls++;
    g_last_sinks_slot = slot_key;
    return TRUE;
}
gboolean sinks_switch_slot(AppData *app, char slot_key) {
    (void)app;
    g_sinks_switch_slot_calls++;
    g_last_sinks_slot = slot_key;
    return TRUE;
}
gboolean proc_signal_selected_with_modifiers(AppData *app, guint state) { (void)app; (void)state; return TRUE; }
void proc_filter(AppData *app, const char *filter) { (void)app; (void)filter; }

void enter_command_mode(AppData *app) { (void)app; }
void exit_command_mode(AppData *app) { if (app) { app->command_mode.state = CMD_MODE_NORMAL; app->active_prefix_claim = '\0'; } }
void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) { (void)app; (void)provider; }
void cofi_exit_modal(AppData *app) { (void)app; }
gboolean cofi_handle_modal_key(AppData *app, GdkEventKey *event) { (void)app; (void)event; return FALSE; }
const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) { (void)prefix; return NULL; }
const CofiTabProvider *cofi_get_provider_for_tab_prefix(char prefix) { (void)prefix; return NULL; }
const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) { (void)tab_mode; return g_provider_for_tab; }
int cofi_get_provider_id_for_tab(int tab_mode) { (void)tab_mode; return 0; }
int cofi_filtered_to_raw(int provider_id, int filtered_idx) { (void)provider_id; return filtered_idx; }
TabMode apps_tab_mode(void) { return TEST_APPS_TAB; }
TabMode harpoon_tab_mode(void) { return TEST_HARPOON_TAB; }

static const char *test_sink_slot_payload(AppData *app, int raw_idx) {
    (void)app; (void)raw_idx;
    return "alsa_output.test";
}

static CofiActionStatus test_sink_slot_recall(AppData *app, const char *payload) {
    (void)app;
    if (payload && strcmp(payload, "alsa_output.test") == 0) {
        g_sinks_switch_slot_calls++;
        g_last_sinks_slot = 'a';
        return COFI_HANDLED_HIDE;
    }
    return COFI_ACTION_ERROR;
}

static void enable_test_sinks_provider(void) {
    memset(&g_sinks_provider, 0, sizeof(g_sinks_provider));
    g_sinks_provider.tab_mode = TEST_SINKS_TAB;
    g_sinks_provider.id = "sinks";
    g_sinks_provider.slot_store_enabled = 1;
    g_sinks_provider.slot_payload_for = test_sink_slot_payload;
    g_sinks_provider.slot_recall = test_sink_slot_recall;
    g_provider_for_tab = &g_sinks_provider;
}

static gboolean test_ctrl_n_handle_key(GdkEventKey *event, AppData *app) {
    (void)app;
    g_provider_handle_key_calls++;
    if ((event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N)) {
        return TRUE;
    }
    return FALSE;
}

static const char *test_ctrl_n_slot_payload(AppData *app, int raw_idx) {
    (void)app;
    (void)raw_idx;
    return "projects:payload";
}

static CofiActionStatus test_ctrl_n_slot_recall(AppData *app, const char *payload) {
    (void)app;
    if (payload && strcmp(payload, "projects:payload") == 0) {
        g_ctrl_n_recall_calls++;
        return COFI_HANDLED_HIDE;
    }
    return COFI_ACTION_ERROR;
}

static void enable_ctrl_n_slot_provider(void) {
    memset(&g_ctrl_n_provider, 0, sizeof(g_ctrl_n_provider));
    g_ctrl_n_provider.tab_mode = TEST_SINKS_TAB;
    g_ctrl_n_provider.id = "projects";
    g_ctrl_n_provider.handle_key = test_ctrl_n_handle_key;
    g_ctrl_n_provider.slot_store_enabled = 1;
    g_ctrl_n_provider.slot_payload_for = test_ctrl_n_slot_payload;
    g_ctrl_n_provider.slot_recall = test_ctrl_n_slot_recall;
    g_provider_for_tab = &g_ctrl_n_provider;
}

WindowInfo *get_selected_window(AppData *app) {
    if (!app || app->filtered_count <= 0) return NULL;
    if (app->selection.window_index < 0 || app->selection.window_index >= app->filtered_count) return NULL;
    return &app->filtered[app->selection.window_index];
}

void move_selection_up(AppData *app) { (void)app; }
void move_selection_down(AppData *app) { (void)app; }
int get_selected_index(AppData *app) { (void)app; return 0; }
void handle_repeat_key(AppData *app) { (void)app; }
void store_last_windows_query(AppData *app, const char *query) { (void)app; (void)query; }

void set_workspace_switch_state(int state) { g_workspace_switch_state = state; }

void activate_window(Display *display, Window window_id) {
    (void)display;
    g_activate_calls++;
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
    g_last_switch_desktop = desktop;
}

int get_number_of_desktops(Display *display) { (void)display; return g_workspace_count; }

void assign_workspace_slots(AppData *app) {
    g_assign_workspace_slots_calls++;
    (void)app;
}

Window get_workspace_slot_window(const WorkspaceSlotManager *manager, int slot) {
    g_get_workspace_slot_calls++;
    g_last_workspace_slot_query = slot;
    if (!manager || slot < 1 || slot > MAX_WORKSPACE_SLOTS) return 0;
    return manager->slots[slot - 1].id;
}

Window get_slot_window(const HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return 0;
    if (!manager->slots[slot].assigned || !manager->matching) return 0;
    for (int i = 0; i < manager->matching->count; i++) {
        if (manager->matching->entries[i].match_id == manager->slots[slot].match_id) {
            return manager->matching->entries[i].bound_x11_id;
        }
    }
    return 0;
}

int get_window_slot(const HarpoonManager *manager, Window id) {
    if (!manager || !manager->matching) return -1;
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (!manager->slots[i].assigned) continue;
        for (int j = 0; j < manager->matching->count; j++) {
            if (manager->matching->entries[j].match_id == manager->slots[i].match_id &&
                manager->matching->entries[j].bound_x11_id == id) {
                return i;
            }
        }
    }
    return -1;
}

void unassign_slot(HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return;
    memset(&manager->slots[slot], 0, sizeof(manager->slots[slot]));
}

int matching_run_gc(AppData *app) { (void)app; g_matching_run_gc_calls++; return 0; }

void match_entry_delete_by_match_id(MatchEntryManager *manager, int match_id) {
    int idx = -1;
    if (manager) {
        for (int i = 0; i < manager->count; i++) {
            if (manager->entries[i].match_id == match_id) {
                idx = i;
                break;
            }
        }
    }
    if (idx < 0) return;
    g_delete_by_match_id_calls++;
    g_last_deleted_match_id = match_id;
    for (int i = idx; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    memset(&manager->entries[manager->count - 1], 0, sizeof(manager->entries[0]));
    manager->count--;
}

void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window) {
    if (!manager || !window || slot < 0 || slot >= MAX_HARPOON_SLOTS || !manager->matching) return;
    int match_id = 0;
    for (int i = 0; i < manager->matching->count; i++) {
        if (manager->matching->entries[i].bound_x11_id == window->id) {
            match_id = manager->matching->entries[i].match_id;
            break;
        }
    }
    if (match_id == 0 && manager->matching->count < MAX_WINDOWS) {
        int idx = manager->matching->count++;
        manager->matching->entries[idx].match_id = idx + 1;
        manager->matching->entries[idx].bound_x11_id = window->id;
        g_strlcpy(manager->matching->entries[idx].original_title, window->title,
                  sizeof(manager->matching->entries[idx].original_title));
        manager->matching->entries[idx].assigned = 1;
        match_id = manager->matching->entries[idx].match_id;
    }
    manager->slots[slot].assigned = 1;
    manager->slots[slot].match_id = match_id;
}

void save_harpoon_slots(const HarpoonManager *manager) { (void)manager; g_save_harpoon_calls++; }
void save_config(const CofiConfig *config) { (void)config; g_save_config_calls++; }
void update_display(AppData *app) { (void)app; g_update_display_calls++; }

void show_name_edit_overlay(AppData *app) { (void)app; }
void show_name_delete_overlay(AppData *app, const char *custom_name, int manager_index) {
    (void)app; (void)custom_name; (void)manager_index;
}
int match_entry_find_index_by_window(const MatchEntryManager *manager, Window id) { (void)manager; (void)id; return -1; }
bool match_entry_matches_window(const MatchEntry *entry, const WindowInfo *window) {
    if (!entry || !window) return false;
    return strcmp(entry->original_title, window->title) == 0;
}
int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) {
    if (!manager || match_id <= 0) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].match_id == match_id) return i;
    }
    return -1;
}
void save_match_entries(const MatchEntryManager *manager) { (void)manager; }
int matching_create_entry(MatchEntryManager *manager, const WindowInfo *w) {
    if (!manager || !w) return -1;
    if (manager->count >= MAX_WINDOWS) return -1;
    int idx = manager->count++;
    manager->entries[idx].match_id = idx + 1;
    manager->entries[idx].bound_x11_id = w->id;
    g_strlcpy(manager->entries[idx].original_title, w->title,
              sizeof(manager->entries[idx].original_title));
    manager->entries[idx].assigned = 1;
    return manager->entries[idx].match_id;
}
void filter_matching(AppData *app, const char *filter) { (void)app; (void)filter; }
MatchEntry *matching_selected_entry(AppData *app) { (void)app; return NULL; }
int matching_selected_manager_index(AppData *app) { (void)app; return -1; }
void show_harpoon_delete_overlay(AppData *app, int slot) { (void)app; (void)slot; }
const char *get_next_enum_value(const char *key, const char *current_value) { (void)key; (void)current_value; return NULL; }
int apply_config_setting(CofiConfig *config, const char *key, const char *value, char *err_buf, size_t err_size) {
    (void)config; (void)key; (void)value; (void)err_buf; (void)err_size; return 0;
}
void filter_config(AppData *app, const char *filter) { (void)app; (void)filter; }
ConfigEntry *config_selected_entry(AppData *app) { (void)app; return NULL; }
void config_select_key(AppData *app, const char *key) { (void)app; (void)key; }
int config_entry_allows_edit(const ConfigEntry *entry) { (void)entry; return 0; }
void show_overlay(AppData *app, OverlayType type, void *data) { (void)app; (void)type; (void)data; }
void cleanup_hotkeys(AppData *app) { (void)app; }
int remove_hotkey_binding(HotkeyConfig *config, const char *key) { (void)config; (void)key; return 0; }
int save_hotkey_config(const HotkeyConfig *config) { (void)config; return 0; }
void regrab_hotkeys(AppData *app) { (void)app; }
int replay_all_rules_against_open_windows(AppData *app) { (void)app; return 0; }
gboolean replay_selected_filtered_rule(AppData *app) { (void)app; return TRUE; }
Rule *rules_selected_rule(AppData *app) { (void)app; return NULL; }
int rules_selected_config_index(AppData *app) { (void)app; return -1; }
void rules_select_config_index(AppData *app, int config_index) { (void)app; (void)config_index; }
void filter_hotkeys(AppData *app, const char *filter) { (void)app; (void)filter; }
HotkeyBinding *hotkeys_selected_binding(AppData *app, int *master_idx_out) {
    (void)app;
    if (master_idx_out) *master_idx_out = -1;
    return NULL;
}
void hotkeys_select_key(AppData *app, const char *key) { (void)app; (void)key; }
void filter_windows(AppData *app, const char *query) { (void)app; (void)query; }
void filter_workspaces(AppData *app, const char *query) { (void)app; (void)query; }
void filter_rules(AppData *app, const char *filter) { (void)app; (void)filter; }
void filter_apps(AppData *app, const char *query) { (void)app; (void)query; }
void reset_selection(AppData *app) { (void)app; }
void preserve_selection(AppData *app) { (void)app; }
void restore_selection(AppData *app) { (void)app; }
void apps_launch(const AppEntry *entry) { (void)entry; }

#include "ui/key_handler.c"

static void reset_captures(void) {
    g_hide_calls = 0;
    g_activate_calls = 0;
    g_last_activate_window = 0;
    g_switch_calls = 0;
    g_last_switch_desktop = -1;
    g_assign_workspace_slots_calls = 0;
    g_save_config_calls = 0;
    g_save_harpoon_calls = 0;
    g_update_display_calls = 0;
    g_workspace_switch_state = 0;
    g_workspace_count = 9;
    g_highlight_calls = 0;
    g_last_highlight_window = 0;
    g_get_workspace_slot_calls = 0;
    g_last_workspace_slot_query = -1;
    g_sinks_assign_calls = 0;
    g_sinks_switch_slot_calls = 0;
    g_last_sinks_slot = '\0';
    g_provider_handle_key_calls = 0;
    g_ctrl_n_recall_calls = 0;
    g_provider_for_tab = NULL;
    g_matching_run_gc_calls = 0;
    g_delete_by_match_id_calls = 0;
    g_last_deleted_match_id = 0;
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->entry = gtk_entry_new();
    app->window_visible = TRUE;
    app->current_tab = TAB_WINDOWS;
    app->filtered_count = 1;
    app->selection.window_index = 0;
    app->filtered[0].id = (Window)0x111;
    strcpy(app->filtered[0].title, "One");
    strcpy(app->filtered[0].class_name, "Class");
    strcpy(app->filtered[0].instance, "Inst");
    strcpy(app->filtered[0].type, "Normal");
    app->harpoon.matching = &app->matching;
}

static GdkEventKey make_key(guint keyval, GdkModifierType state) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = keyval;
    event.state = state;
    return event;
}

static int slot_for_letter(char c) {
    return HARPOON_FIRST_LETTER + (c - 'a');
}

static void test_ctrl_1_assigns_selected_window_to_slot_1(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_1, GDK_CONTROL_MASK);
    gboolean handled = handle_harpoon_assignment(&ev, &app);

    ASSERT_TRUE("Ctrl+1 assignment handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+1 assigns slot 1", app.harpoon.slots[1].assigned == 1);
    ASSERT_TRUE("Ctrl+1 slot 1 resolves selected window id",
                get_slot_window(&app.harpoon, 1) == app.filtered[0].id);
}

static void test_ctrl_1_second_press_unassigns_same_window(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_1, GDK_CONTROL_MASK);
    handle_harpoon_assignment(&ev, &app);
    gboolean handled = handle_harpoon_assignment(&ev, &app);

    ASSERT_TRUE("Ctrl+1 re-press handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+1 re-press unassigns slot 1", app.harpoon.slots[1].assigned == 0);
    ASSERT_TRUE("Ctrl+1 re-press deletes owned match entry",
                g_delete_by_match_id_calls == 1 &&
                g_last_deleted_match_id > 0 &&
                app.matching.count == 0);
}

static void test_ctrl_j_without_shift_not_assignment(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    HarpoonManager before = app.harpoon;
    GdkEventKey ev = make_key(GDK_KEY_j, GDK_CONTROL_MASK);
    gboolean handled = handle_harpoon_assignment(&ev, &app);

    ASSERT_TRUE("Ctrl+j without Shift not treated as assignment", handled == FALSE);
    ASSERT_TRUE("Ctrl+j without Shift keeps all slots unchanged",
                memcmp(&before, &app.harpoon, sizeof(HarpoonManager)) == 0);
}

static void test_ctrl_shift_j_assigns_letter_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    int j_slot = slot_for_letter('j');
    GdkEventKey ev = make_key(GDK_KEY_j, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    gboolean handled = handle_harpoon_assignment(&ev, &app);

    ASSERT_TRUE("Ctrl+Shift+j assignment handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+Shift+j assigns j slot", app.harpoon.slots[j_slot].assigned == 1);
    ASSERT_TRUE("Ctrl+Shift+j slot resolves selected window",
                get_slot_window(&app.harpoon, j_slot) == app.filtered[0].id);
}

static void test_ctrl_5_reassigns_existing_window_from_old_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    Window x = (Window)0xCAFE;
    app.filtered[0].id = x;
    app.matching.count = 1;
    app.matching.entries[0].match_id = 44;
    app.matching.entries[0].bound_x11_id = x;
    g_strlcpy(app.matching.entries[0].original_title, app.filtered[0].title,
              sizeof(app.matching.entries[0].original_title));
    app.matching.entries[0].assigned = 1;
    app.harpoon.slots[3].assigned = 1;
    app.harpoon.slots[3].match_id = 44;

    GdkEventKey ev = make_key(GDK_KEY_5, GDK_CONTROL_MASK);
    gboolean handled = handle_harpoon_assignment(&ev, &app);

    ASSERT_TRUE("Ctrl+5 reassignment handled", handled == TRUE);
    ASSERT_TRUE("Reassignment clears old slot 3", app.harpoon.slots[3].assigned == 0);
    ASSERT_TRUE("Reassignment sets new slot 5", app.harpoon.slots[5].assigned == 1);
    ASSERT_TRUE("Reassignment new slot 5 resolves selected window", get_slot_window(&app.harpoon, 5) == x);
    ASSERT_TRUE("Reassignment deletes displaced old match entry",
                g_delete_by_match_id_calls == 1 &&
                g_last_deleted_match_id == 44 &&
                match_entry_find_index_by_match_id(&app.matching, 44) == -1);
    ASSERT_TRUE("Reassignment keeps new slot match entry",
                app.harpoon.slots[5].match_id > 0 &&
                match_entry_find_index_by_match_id(&app.matching, app.harpoon.slots[5].match_id) >= 0);
}

static void test_alt_1_workspaces_mode_switches_workspace(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.config.digit_slot_mode = DIGIT_MODE_WORKSPACES;
    app.current_tab = TAB_WINDOWS;

    GdkEventKey ev = make_key(GDK_KEY_1, GDK_MOD1_MASK);
    gboolean handled = handle_harpoon_workspace_switching(&ev, &app);

    ASSERT_TRUE("Alt+1 workspaces mode handled", handled == TRUE);
    ASSERT_TRUE("Alt+1 workspaces mode switches to desktop 0",
                g_switch_calls == 1 && g_last_switch_desktop == 0);
}

static void test_alt_1_per_workspace_mode_activates_slot_window(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.config.digit_slot_mode = DIGIT_MODE_PER_WORKSPACE;
    app.current_tab = TAB_WINDOWS;
    app.workspace_slots.slots[0].id = (Window)0xBEEF;

    GdkEventKey ev = make_key(GDK_KEY_1, GDK_MOD1_MASK);
    gboolean handled = handle_harpoon_workspace_switching(&ev, &app);

    ASSERT_TRUE("Alt+1 per-workspace mode handled", handled == TRUE);
    ASSERT_TRUE("Alt+1 per-workspace mode assigns workspace slots first",
                g_assign_workspace_slots_calls == 1);
    ASSERT_TRUE("Alt+1 per-workspace mode queries workspace slot 1",
                g_get_workspace_slot_calls == 1 && g_last_workspace_slot_query == 1);
    ASSERT_TRUE("Alt+1 per-workspace mode activates slot-1 window",
                g_activate_calls == 1 && g_last_activate_window == (Window)0xBEEF);
    ASSERT_TRUE("Alt+1 per-workspace mode highlights target window",
                g_highlight_calls == 1 && g_last_highlight_window == (Window)0xBEEF);
    ASSERT_TRUE("Alt+1 per-workspace mode hides window",
                g_hide_calls == 1 && app.window_visible == FALSE);
}

static void test_alt_a_default_mode_activates_harpoon_letter_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    int a_slot = slot_for_letter('a');
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.current_tab = TAB_WINDOWS;
    app.matching.count = 1;
    app.matching.entries[0].match_id = 91;
    app.matching.entries[0].bound_x11_id = (Window)0xA11;
    app.matching.entries[0].assigned = 1;
    app.harpoon.slots[a_slot].assigned = 1;
    app.harpoon.slots[a_slot].match_id = 91;

    GdkEventKey ev = make_key(GDK_KEY_a, GDK_MOD1_MASK);
    gboolean handled = handle_harpoon_workspace_switching(&ev, &app);

    ASSERT_TRUE("Alt+a default mode handled", handled == TRUE);
    ASSERT_TRUE("Alt+a default mode activates harpoon letter slot",
                g_activate_calls == 1 && g_last_activate_window == (Window)0xA11);
}

static void test_ctrl_a_on_sinks_assigns_sink_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_SINKS_TAB;
    enable_test_sinks_provider();

    GdkEventKey ev = make_key(GDK_KEY_a, GDK_CONTROL_MASK);
    gboolean handled = handle_harpoon_assignment(&ev, &app);

    ASSERT_TRUE("Ctrl+a on Sinks handled", handled == TRUE);
    ASSERT_TRUE("Ctrl+a on Sinks assigns provider slot",
                strcmp(slot_lookup(&app.harpoon.store, "sinks", 'a'),
                       "alsa_output.test") == 0);
}

static void test_alt_a_on_sinks_switches_sink_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_SINKS_TAB;
    slot_assign(&app.harpoon.store, 'a', "sinks", "alsa_output.test");
    enable_test_sinks_provider();

    GdkEventKey ev = make_key(GDK_KEY_a, GDK_MOD1_MASK);
    gboolean handled = handle_harpoon_workspace_switching(&ev, &app);

    ASSERT_TRUE("Alt+a on Sinks handled", handled == TRUE);
    ASSERT_TRUE("Alt+a on Sinks delegates to sink recall",
                g_sinks_switch_slot_calls == 1 && g_last_sinks_slot == 'a');
}

static void test_ctrl_j_on_sinks_remains_navigation_key(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_SINKS_TAB;

    GdkEventKey ev = make_key(GDK_KEY_j, GDK_CONTROL_MASK);
    gboolean handled = handle_harpoon_assignment(&ev, &app);

    ASSERT_TRUE("Ctrl+j on Sinks not a slot assignment", handled == FALSE);
    ASSERT_TRUE("Ctrl+j on Sinks does not call sink assignment",
                g_sinks_assign_calls == 0);
}

static void test_provider_ctrl_n_shortcut_preempts_provider_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_SINKS_TAB;
    enable_ctrl_n_slot_provider();

    GdkEventKey ev = make_key(GDK_KEY_n, GDK_CONTROL_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Provider Ctrl+n shortcut handled", handled == TRUE);
    ASSERT_TRUE("Provider handle_key saw Ctrl+n", g_provider_handle_key_calls == 1);
    ASSERT_TRUE("Ctrl+n did not assign provider slot n",
                slot_lookup(&app.harpoon.store, "projects", 'n') == NULL);
}

static void test_ctrl_shift_n_falls_through_to_provider_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_SINKS_TAB;
    enable_ctrl_n_slot_provider();

    GdkEventKey ev = make_key(GDK_KEY_n, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Ctrl+Shift+n falls through and is handled", handled == TRUE);
    ASSERT_TRUE("Provider handle_key saw Ctrl+Shift+n", g_provider_handle_key_calls == 1);
    ASSERT_TRUE("Ctrl+Shift+n assigns provider slot n",
                strcmp(slot_lookup(&app.harpoon.store, "projects", 'n'),
                       "projects:payload") == 0);
}

static void test_alt_n_falls_through_to_provider_slot_recall(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_SINKS_TAB;
    enable_ctrl_n_slot_provider();
    slot_assign(&app.harpoon.store, 'n', "projects", "projects:payload");

    GdkEventKey ev = make_key(GDK_KEY_n, GDK_MOD1_MASK);
    gboolean handled = on_key_press(NULL, &ev, &app);

    ASSERT_TRUE("Alt+n falls through and is handled", handled == TRUE);
    ASSERT_TRUE("Provider handle_key saw Alt+n", g_provider_handle_key_calls == 1);
    ASSERT_TRUE("Alt+n recalls provider slot n", g_ctrl_n_recall_calls == 1);
    ASSERT_TRUE("Alt+n recall hides window", g_hide_calls == 1);
}

static void test_alt_digit_out_of_workspace_range_non_windows_noop(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TEST_WORKSPACES_TAB;
    app.workspace_count = 2;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;

    GdkEventKey ev = make_key(GDK_KEY_9, GDK_MOD1_MASK);
    gboolean handled = handle_harpoon_workspace_switching(&ev, &app);

    ASSERT_TRUE("Alt+digit out of range on non-Windows tab returns FALSE", handled == FALSE);
    ASSERT_TRUE("Alt+digit out of range on non-Windows tab does not call stubs",
                g_switch_calls == 0 && g_activate_calls == 0 && g_hide_calls == 0);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Key handler harpoon tests\n");
        printf("=========================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Key handler harpoon tests\n");
    printf("=========================\n\n");

    test_ctrl_1_assigns_selected_window_to_slot_1();
    test_ctrl_1_second_press_unassigns_same_window();
    test_ctrl_j_without_shift_not_assignment();
    test_ctrl_shift_j_assigns_letter_slot();
    test_ctrl_5_reassigns_existing_window_from_old_slot();
    test_alt_1_workspaces_mode_switches_workspace();
    test_alt_1_per_workspace_mode_activates_slot_window();
    test_alt_a_default_mode_activates_harpoon_letter_slot();
    test_ctrl_a_on_sinks_assigns_sink_slot();
    test_alt_a_on_sinks_switches_sink_slot();
    test_ctrl_j_on_sinks_remains_navigation_key();
    test_provider_ctrl_n_shortcut_preempts_provider_slot();
    test_ctrl_shift_n_falls_through_to_provider_slot();
    test_alt_n_falls_through_to_provider_slot_recall();
    test_alt_digit_out_of_workspace_range_non_windows_noop();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
