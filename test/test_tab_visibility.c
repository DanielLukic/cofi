#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/daemon_socket.h"
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

static int entry_clear_calls = 0;
static int reset_selection_calls = 0;
static int update_display_calls = 0;
static int show_window_calls = 0;
static int exit_command_mode_calls = 0;

void gtk_entry_set_text(GtkEntry *entry, const gchar *text) {
    (void)entry;
    if (text && text[0] == '\0') {
        entry_clear_calls++;
    }
}

void gtk_entry_set_placeholder_text(GtkEntry *entry, const gchar *text) {
    (void)entry;
    (void)text;
}

void gtk_widget_grab_focus(GtkWidget *widget) {
    (void)widget;
}

void gtk_text_buffer_set_text(GtkTextBuffer *buffer, const gchar *text, gint len) {
    (void)buffer;
    (void)text;
    (void)len;
}

void reset_selection(AppData *app) {
    (void)app;
    reset_selection_calls++;
}

void update_display(AppData *app) {
    (void)app;
    update_display_calls++;
}

void filter_windows(AppData *app, const char *filter) {
    (void)app;
    (void)filter;
}

void filter_names(AppData *app, const char *filter) {
    (void)app;
    (void)filter;
}

void apps_filter(const char *query, AppEntry *results, int *count) {
    (void)query;
    (void)results;
    if (count) {
        *count = 0;
    }
}

gboolean has_match(const char *query, const char *text) {
    (void)query;
    (void)text;
    return FALSE;
}

void build_config_entries(const CofiConfig *config, ConfigEntry entries[], int *count) {
    (void)config;
    (void)entries;
    if (count) {
        *count = 0;
    }
}

void apps_load(void) {
}

void path_binaries_ensure_loaded(AppData *app) {
    (void)app;
}

void path_binaries_filter(const char *query, AppEntry *out, int *out_count) {
    (void)query;
    (void)out;
    if (out_count) {
        *out_count = 0;
    }
}

gboolean path_binaries_is_scanning(void) {
    return FALSE;
}

void dispatch_hotkey_mode(AppData *app, ShowMode mode) {
    (void)app;
    (void)mode;
}

void exit_command_mode(AppData *app) {
    (void)app;
    exit_command_mode_calls++;
}

void show_help_commands(AppData *app) {
    (void)app;
}

void save_config(const CofiConfig *config) {
    (void)config;
}

int apply_config_setting(CofiConfig *config, const char *key, const char *value,
                         char *error, size_t error_size) {
    (void)config;
    (void)key;
    (void)value;
    (void)error;
    (void)error_size;
    return 0;
}

int parse_hotkey_command(const char *args, char *key, size_t key_size,
                         char *cmd, size_t cmd_size) {
    (void)args;
    (void)key;
    (void)key_size;
    (void)cmd;
    (void)cmd_size;
    return 0;
}

int add_hotkey_binding(HotkeyConfig *config, const char *key, const char *command) {
    (void)config;
    (void)key;
    (void)command;
    return 1;
}

gboolean remove_hotkey_binding(HotkeyConfig *config, const char *key) {
    (void)config;
    (void)key;
    return FALSE;
}

gboolean save_hotkey_config(const HotkeyConfig *config) {
    (void)config;
    return TRUE;
}

void regrab_hotkeys(AppData *app) {
    (void)app;
}

int format_hotkey_display(const HotkeyConfig *config, char *buffer, size_t size) {
    (void)config;
    if (size > 0) {
        buffer[0] = '\0';
    }
    return 0;
}

#define STUB_UI_HANDLER(name) \
    gboolean name(AppData *app, WindowInfo *window, const char *args) { \
        (void)app; (void)window; (void)args; return FALSE; \
    }

STUB_UI_HANDLER(cmd_always_below)
STUB_UI_HANDLER(cmd_assign_name)
STUB_UI_HANDLER(cmd_assign_slots)
STUB_UI_HANDLER(cmd_always_on_top)
STUB_UI_HANDLER(cmd_close_window)
STUB_UI_HANDLER(cmd_change_workspace)
STUB_UI_HANDLER(cmd_every_workspace)
STUB_UI_HANDLER(cmd_horizontal_maximize)
STUB_UI_HANDLER(cmd_jump_workspace)
STUB_UI_HANDLER(cmd_move_all_to_workspace)
STUB_UI_HANDLER(cmd_minimize_window)
STUB_UI_HANDLER(cmd_mouse)
STUB_UI_HANDLER(cmd_maximize_window)
STUB_UI_HANDLER(cmd_pull_window)
STUB_UI_HANDLER(cmd_rename_workspace)
STUB_UI_HANDLER(cmd_skip_taskbar)
STUB_UI_HANDLER(cmd_swap_windows)
STUB_UI_HANDLER(cmd_toggle_monitor)
STUB_UI_HANDLER(cmd_tile_window)
STUB_UI_HANDLER(cmd_vertical_maximize)

void show_window(AppData *app) {
    (void)app;
    show_window_calls++;
}

void enter_command_mode(AppData *app) {
    (void)app;
}

void enter_run_mode(AppData *app, const char *prefill_command) {
    (void)app;
    (void)prefill_command;
}

void exit_run_mode(AppData *app) {
    (void)app;
}

int get_active_window_id(Display *display) {
    (void)display;
    return 0;
}

#ifdef GTK_ENTRY
#undef GTK_ENTRY
#endif
#define GTK_ENTRY(widget) ((GtkEntry *)(widget))

#include "../src/tab_switching.c"
#include "../src/command_handlers_ui.c"
#include "../src/daemon_socket_runtime.c"

static void reset_counters(void) {
    entry_clear_calls = 0;
    reset_selection_calls = 0;
    update_display_calls = 0;
    show_window_calls = 0;
    exit_command_mode_calls = 0;
}

static AppData make_app(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = (GtkWidget *)0x1;
    app.current_tab = TAB_WINDOWS;
    return app;
}

static AppData make_default_visibility_app(void) {
    AppData app = make_app();

    for (int i = TAB_WINDOWS; i <= TAB_APPS; i++) {
        app.tab_visibility[i] = TAB_VIS_HIDDEN;
    }
    app.tab_visibility[TAB_WINDOWS] = TAB_VIS_PINNED;
    app.tab_visibility[TAB_APPS] = TAB_VIS_PINNED;

    return app;
}

static void test_tab_switching_forward_cycles_all_tabs(void) {
    AppData app = make_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;

    TabMode expected[] = {
        TAB_WORKSPACES,
        TAB_HARPOON,
        TAB_NAMES,
        TAB_CONFIG,
        TAB_HOTKEYS,
        TAB_APPS,
        TAB_WINDOWS
    };

    for (int i = 0; i < 7; i++) {
        gboolean handled = handle_tab_switching(&event, &app);
        ASSERT_TRUE("forward tab switch handled", handled == TRUE);

        char msg[64];
        snprintf(msg, sizeof(msg), "forward step %d reaches expected tab", i + 1);
        ASSERT_TRUE(msg, app.current_tab == expected[i]);
    }
}

static void test_tab_switching_backward_cycles_all_tabs(void) {
    AppData app = make_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;
    event.state = GDK_SHIFT_MASK;

    TabMode expected[] = {
        TAB_APPS,
        TAB_HOTKEYS,
        TAB_CONFIG,
        TAB_NAMES,
        TAB_HARPOON,
        TAB_WORKSPACES,
        TAB_WINDOWS
    };

    for (int i = 0; i < 7; i++) {
        gboolean handled = handle_tab_switching(&event, &app);
        ASSERT_TRUE("backward tab switch handled", handled == TRUE);

        char msg[64];
        snprintf(msg, sizeof(msg), "backward step %d reaches expected tab", i + 1);
        ASSERT_TRUE(msg, app.current_tab == expected[i]);
    }
}

static void test_switch_to_tab_updates_state_and_clears_entry(void) {
    AppData app = make_app();

    for (int tab = TAB_WINDOWS; tab <= TAB_APPS; tab++) {
        reset_counters();
        app.current_tab = (tab == TAB_WINDOWS) ? TAB_APPS : TAB_WINDOWS;

        switch_to_tab(&app, (TabMode)tab);

        ASSERT_TRUE("switch_to_tab updates current_tab", app.current_tab == (TabMode)tab);
        ASSERT_TRUE("switch_to_tab clears entry", entry_clear_calls == 1);
        ASSERT_TRUE("switch_to_tab resets selection", reset_selection_calls == 1);
        ASSERT_TRUE("switch_to_tab refreshes display", update_display_calls == 1);
    }
}

static void test_cmd_show_names_switches_to_names_tab(void) {
    AppData app = make_app();
    app.current_tab = TAB_WINDOWS;

    reset_counters();
    gboolean result = cmd_show(&app, NULL, "names");

    ASSERT_TRUE("cmd_show names returns FALSE", result == FALSE);
    ASSERT_TRUE("cmd_show names exits command mode", exit_command_mode_calls == 1);
    ASSERT_TRUE("cmd_show names switches tab", app.current_tab == TAB_NAMES);
}

static void test_daemon_opcode_harpoon_switches_to_harpoon_tab(void) {
    AppData app = make_app();
    app.command_mode.state = CMD_MODE_NORMAL;

    reset_counters();
    daemon_socket_dispatch_opcode(&app, COFI_OPCODE_HARPOON);

    ASSERT_TRUE("daemon opcode harpoon shows window", show_window_calls == 1);
    ASSERT_TRUE("daemon opcode harpoon switches tab", app.current_tab == TAB_HARPOON);
}

static void test_surface_tab_surfaces_hidden_tab(void) {
    AppData app = make_default_visibility_app();

    ASSERT_TRUE("workspaces starts hidden", tab_is_visible(&app, TAB_WORKSPACES) == FALSE);

    surface_tab(&app, TAB_WORKSPACES);

    ASSERT_TRUE("surface_tab marks tab visible", tab_is_visible(&app, TAB_WORKSPACES) == TRUE);
    ASSERT_TRUE("surface_tab switches current tab", app.current_tab == TAB_WORKSPACES);
}

static void test_tab_switching_skips_hidden_tabs(void) {
    AppData app = make_default_visibility_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;

    gboolean handled = handle_tab_switching(&event, &app);
    ASSERT_TRUE("tab switch handled with hidden tabs", handled == TRUE);
    ASSERT_TRUE("forward skips hidden to apps", app.current_tab == TAB_APPS);

    handled = handle_tab_switching(&event, &app);
    ASSERT_TRUE("tab switch wraps pinned tabs", handled == TRUE);
    ASSERT_TRUE("forward wraps back to windows", app.current_tab == TAB_WINDOWS);
}

static void test_tab_switching_clears_surfaced_tabs_on_pinned_return(void) {
    AppData app = make_default_visibility_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;
    event.state = GDK_SHIFT_MASK;

    surface_tab(&app, TAB_WORKSPACES);
    ASSERT_TRUE("workspaces surfaced before cycling", app.tab_visibility[TAB_WORKSPACES] == TAB_VIS_SURFACED);

    gboolean handled = handle_tab_switching(&event, &app);
    ASSERT_TRUE("shift-tab handled from surfaced tab", handled == TRUE);
    ASSERT_TRUE("shift-tab moved to pinned windows", app.current_tab == TAB_WINDOWS);
    ASSERT_TRUE("surfaced tab hidden again after leaving", app.tab_visibility[TAB_WORKSPACES] == TAB_VIS_HIDDEN);
}

int main(void) {
    printf("Tab visibility safety-net tests\n");
    printf("===============================\n\n");

    test_tab_switching_forward_cycles_all_tabs();
    test_tab_switching_backward_cycles_all_tabs();
    test_switch_to_tab_updates_state_and_clears_entry();
    test_cmd_show_names_switches_to_names_tab();
    test_daemon_opcode_harpoon_switches_to_harpoon_tab();
    test_surface_tab_surfaces_hidden_tab();
    test_tab_switching_skips_hidden_tabs();
    test_tab_switching_clears_surfaced_tabs_on_pinned_return();

    printf("\n===============================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
