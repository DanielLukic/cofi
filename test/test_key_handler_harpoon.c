#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../src/app_data.h"
#include "../src/key_handler.h"
#include "../src/constants.h"

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

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

/* --- Stubs required by key_handler.c --- */
gboolean is_overlay_active(AppData *app) { (void)app; return FALSE; }
gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event) { (void)app; (void)event; return FALSE; }
gboolean handle_command_key(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }
gboolean handle_run_key(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }
gboolean handle_tab_switching(GdkEventKey *event, AppData *app) { (void)event; (void)app; return FALSE; }

void enter_command_mode(AppData *app) { (void)app; }
void enter_run_mode(AppData *app, const char *prefill_command) { (void)app; (void)prefill_command; }
void handle_run_entry_changed(GtkEntry *entry, AppData *app) { (void)entry; (void)app; }

WindowInfo *get_selected_window(AppData *app) {
    if (!app || app->filtered_count <= 0) return NULL;
    if (app->selection.window_index < 0 || app->selection.window_index >= app->filtered_count) return NULL;
    return &app->filtered[app->selection.window_index];
}

WorkspaceInfo *get_selected_workspace(AppData *app) { (void)app; return NULL; }
void move_selection_up(AppData *app) { (void)app; }
void move_selection_down(AppData *app) { (void)app; }
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
    return manager->slots[slot].assigned ? manager->slots[slot].id : 0;
}

int get_window_slot(const HarpoonManager *manager, Window id) {
    if (!manager) return -1;
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (manager->slots[i].assigned && manager->slots[i].id == id) {
            return i;
        }
    }
    return -1;
}

void unassign_slot(HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return;
    memset(&manager->slots[slot], 0, sizeof(manager->slots[slot]));
}

void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window) {
    if (!manager || !window || slot < 0 || slot >= MAX_HARPOON_SLOTS) return;
    manager->slots[slot].assigned = 1;
    manager->slots[slot].id = window->id;
    strncpy(manager->slots[slot].title, window->title, sizeof(manager->slots[slot].title) - 1);
    strncpy(manager->slots[slot].class_name, window->class_name, sizeof(manager->slots[slot].class_name) - 1);
    strncpy(manager->slots[slot].instance, window->instance, sizeof(manager->slots[slot].instance) - 1);
    strncpy(manager->slots[slot].type, window->type, sizeof(manager->slots[slot].type) - 1);
}

void save_harpoon_slots(const HarpoonManager *manager) { (void)manager; g_save_harpoon_calls++; }
void save_config(const CofiConfig *config) { (void)config; g_save_config_calls++; }
void update_display(AppData *app) { (void)app; g_update_display_calls++; }

void show_name_edit_overlay(AppData *app) { (void)app; }
int find_named_window_by_name(const NamedWindowManager *manager, const char *custom_name) { (void)manager; (void)custom_name; return -1; }
void delete_custom_name(NamedWindowManager *manager, int index) { (void)manager; (void)index; }
void save_named_windows(const NamedWindowManager *manager) { (void)manager; }
void filter_names(AppData *app, const char *filter) { (void)app; (void)filter; }
void show_harpoon_delete_overlay(AppData *app, int slot) { (void)app; (void)slot; }
void show_harpoon_edit_overlay(AppData *app, int slot) { (void)app; (void)slot; }
const char *get_next_enum_value(const char *key, const char *current_value) { (void)key; (void)current_value; return NULL; }
int apply_config_setting(CofiConfig *config, const char *key, const char *value, char *err_buf, size_t err_size) {
    (void)config; (void)key; (void)value; (void)err_buf; (void)err_size; return 0;
}
void filter_config(AppData *app, const char *filter) { (void)app; (void)filter; }
void show_overlay(AppData *app, OverlayType type, void *data) { (void)app; (void)type; (void)data; }
void cleanup_hotkeys(AppData *app) { (void)app; }
int remove_hotkey_binding(HotkeyConfig *config, const char *key) { (void)config; (void)key; return 0; }
int save_hotkey_config(const HotkeyConfig *config) { (void)config; return 0; }
void regrab_hotkeys(AppData *app) { (void)app; }
void filter_hotkeys(AppData *app, const char *filter) { (void)app; (void)filter; }
void filter_windows(AppData *app, const char *query) { (void)app; (void)query; }
void filter_workspaces(AppData *app, const char *query) { (void)app; (void)query; }
void filter_harpoon(AppData *app, const char *filter) { (void)app; (void)filter; }
void filter_apps(AppData *app, const char *query) { (void)app; (void)query; }
void reset_selection(AppData *app) { (void)app; }
void apps_launch(const AppEntry *entry) { (void)entry; }

#include "../src/key_handler.c"

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
    ASSERT_TRUE("Ctrl+1 slot 1 gets selected window id", app.harpoon.slots[1].id == app.filtered[0].id);
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
    ASSERT_TRUE("Ctrl+Shift+j slot stores selected window", app.harpoon.slots[j_slot].id == app.filtered[0].id);
}

static void test_ctrl_5_reassigns_existing_window_from_old_slot(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    Window x = (Window)0xCAFE;
    app.filtered[0].id = x;
    app.harpoon.slots[3].assigned = 1;
    app.harpoon.slots[3].id = x;

    GdkEventKey ev = make_key(GDK_KEY_5, GDK_CONTROL_MASK);
    gboolean handled = handle_harpoon_assignment(&ev, &app);

    ASSERT_TRUE("Ctrl+5 reassignment handled", handled == TRUE);
    ASSERT_TRUE("Reassignment clears old slot 3", app.harpoon.slots[3].assigned == 0);
    ASSERT_TRUE("Reassignment sets new slot 5", app.harpoon.slots[5].assigned == 1);
    ASSERT_TRUE("Reassignment new slot 5 has selected window", app.harpoon.slots[5].id == x);
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
    app.harpoon.slots[a_slot].assigned = 1;
    app.harpoon.slots[a_slot].id = (Window)0xA11;

    GdkEventKey ev = make_key(GDK_KEY_a, GDK_MOD1_MASK);
    gboolean handled = handle_harpoon_workspace_switching(&ev, &app);

    ASSERT_TRUE("Alt+a default mode handled", handled == TRUE);
    ASSERT_TRUE("Alt+a default mode activates harpoon letter slot",
                g_activate_calls == 1 && g_last_activate_window == (Window)0xA11);
}

static void test_alt_digit_out_of_workspace_range_non_windows_noop(void) {
    AppData app;
    init_app(&app);
    reset_captures();

    app.current_tab = TAB_WORKSPACES;
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
    test_alt_digit_out_of_workspace_range_non_windows_noop();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
