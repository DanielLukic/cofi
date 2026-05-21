#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/command_registry.h"
#include "../src/core_commands.h"
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
static int disabled_provider_tab = -1;

#define TEST_WORKSPACES_TAB ((TabMode)(TAB_COUNT + 1))
#define TEST_HARPOON_TAB    ((TabMode)(TAB_COUNT + 2))
#define TEST_APPS_TAB       ((TabMode)(TAB_COUNT + 3))
#define TEST_NAMES_TAB      ((TabMode)(TAB_COUNT + 4))
#define TEST_CONFIG_TAB     ((TabMode)(TAB_COUNT + 5))
#define TEST_HOTKEYS_TAB    ((TabMode)(TAB_COUNT + 6))
#define TEST_RULES_TAB      ((TabMode)(TAB_COUNT + 7))
#define TEST_PROJECTS_TAB   ((TabMode)(TAB_COUNT + 8))

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
    if (!query || !*query) return TRUE;
    if (!text) return FALSE;
    return strstr(text, query) != NULL;
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

void sinks_start_polling(AppData *app) {
    (void)app;
}

void sinks_stop_polling(AppData *app) {
    (void)app;
}
gboolean sinks_switch_slot(AppData *app, char slot_key) {
    (void)app;
    (void)slot_key;
    return FALSE;
}
void proc_start_polling(AppData *app) {
    (void)app;
}
void proc_stop_polling(AppData *app) {
    (void)app;
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

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    (void)app; (void)provider;
}

void cofi_exit_modal(AppData *app) {
    (void)app;
}

gboolean cofi_handle_modal_key(AppData *app, GdkEventKey *event) {
    (void)app; (void)event;
    return FALSE;
}

static void test_apps_on_surface(AppData *app) {
    if (app) {
        app->apps_mode = APPS_MODE_DEFAULT;
    }
}

TabMode apps_tab_mode(void) {
    return TEST_APPS_TAB;
}

TabMode names_tab_mode(void) {
    return TEST_NAMES_TAB;
}

TabMode harpoon_tab_mode(void) {
    return TEST_HARPOON_TAB;
}

TabMode workspaces_tab_mode(void) {
    return TEST_WORKSPACES_TAB;
}

const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) {
    (void)prefix; return NULL;
}

const CofiTabProvider *cofi_get_provider_for_delegate_opcode(int opcode) {
    switch (opcode) {
        case COFI_OPCODE_WORKSPACES:
            return cofi_get_provider_for_tab(TEST_WORKSPACES_TAB);
        case COFI_OPCODE_HARPOON:
            return cofi_get_provider_for_tab(TEST_HARPOON_TAB);
        case COFI_OPCODE_NAMES:
            return cofi_get_provider_for_tab(TEST_NAMES_TAB);
        case COFI_OPCODE_APPLICATIONS:
            return cofi_get_provider_for_tab(TEST_APPS_TAB);
        default:
            return NULL;
    }
}

static gboolean noop_provider_command(AppData *app, WindowInfo *window, const char *args) {
    (void)app;
    (void)window;
    (void)args;
    return FALSE;
}

static const CommandSpec s_apps_command = {
    .primary = "apps",
    .aliases = {"applications", "app", NULL},
    .owner_provider_id = "apps",
    .handler = noop_provider_command,
    .description = "Show applications",
    .help_format = "apps, applications, app"
};

static const CommandSpec s_names_command = {
    .primary = "names",
    .aliases = {"nm", NULL},
    .owner_provider_id = "names",
    .handler = noop_provider_command,
    .description = "Show named windows",
    .help_format = "names, nm"
};

static const CommandSpec s_tab_rules_command = {
    .primary = "rules",
    .aliases = {"rl", NULL},
    .owner_provider_id = "rules",
    .handler = noop_provider_command,
    .description = "Show rules",
    .help_format = "rules, rl"
};

static const CommandSpec s_workspaces_command = {
    .primary = "workspaces",
    .aliases = {"ws", NULL},
    .owner_provider_id = "workspaces",
    .handler = noop_provider_command,
    .description = "Show workspaces",
    .help_format = "workspaces, ws"
};

static void register_tab_visibility_commands(void) {
    cofi_command_registry_reset();
    cofi_register_core_commands();
    cofi_register_command(&s_apps_command);
    cofi_register_command(&s_names_command);
    cofi_register_command(&s_tab_rules_command);
    cofi_register_command(&s_workspaces_command);
}

const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    static CofiTabProvider provider;
    if (tab_mode == TAB_WINDOWS || tab_mode == disabled_provider_tab) return NULL;
    memset(&provider, 0, sizeof(provider));
    provider.tab_mode = tab_mode;
    if (tab_mode == TEST_APPS_TAB) {
        provider.id = "apps";
        provider.on_surface = test_apps_on_surface;
        return &provider;
    }
    if (tab_mode == TEST_RULES_TAB) {
        provider.id = "rules";
        return &provider;
    }
    if (tab_mode == TEST_CONFIG_TAB) {
        provider.id = "config";
        return &provider;
    }
    if (tab_mode == TEST_HOTKEYS_TAB) {
        provider.id = "hotkeys";
        return &provider;
    }
    if (tab_mode == TEST_HARPOON_TAB) {
        provider.id = "harpoon";
        return &provider;
    }
    if (tab_mode == TEST_WORKSPACES_TAB) {
        provider.id = "workspaces";
        return &provider;
    }
    if (tab_mode == TEST_NAMES_TAB) {
        provider.id = "names";
    } else if (tab_mode == TEST_PROJECTS_TAB) {
        provider.id = "projects";
    } else {
        provider.id = "test";
    }
    return &provider;
}
int cofi_get_provider_id(const char *id) {
    if (!id) return -1;
    if (strcmp(id, "apps") == 0) return TEST_APPS_TAB;
    if (strcmp(id, "config") == 0) return TEST_CONFIG_TAB;
    if (strcmp(id, "harpoon") == 0) return TEST_HARPOON_TAB;
    if (strcmp(id, "hotkeys") == 0) return TEST_HOTKEYS_TAB;
    if (strcmp(id, "names") == 0) return TEST_NAMES_TAB;
    if (strcmp(id, "rules") == 0) return TEST_RULES_TAB;
    if (strcmp(id, "workspaces") == 0) return TEST_WORKSPACES_TAB;
    if (strcmp(id, "projects") == 0) return TEST_PROJECTS_TAB;
    return -1;
}
int cofi_provider_is_enabled(int provider_id) {
    return provider_id >= 0 && provider_id != disabled_provider_tab;
}
int cofi_provider_count(void) { return 0; }
const CofiTabProvider *cofi_get_provider(int provider_id) {
    return cofi_get_provider_for_tab(provider_id);
}
int cofi_get_provider_id_for_tab(int tab_mode) { (void)tab_mode; return -1; }
TabMode config_tab_mode(void) { return TEST_CONFIG_TAB; }
TabMode hotkeys_tab_mode(void) { return TEST_HOTKEYS_TAB; }
int cofi_list_provider_tabs(int *tabs, int max_tabs) {
    int count = 0;
    if (count < max_tabs) {
        tabs[count++] = TEST_WORKSPACES_TAB;
    }
    if (count < max_tabs) {
        tabs[count++] = TEST_HARPOON_TAB;
    }
    if (count < max_tabs) {
        tabs[count++] = TEST_APPS_TAB;
    }
    if (count < max_tabs) {
        tabs[count++] = TEST_NAMES_TAB;
    }
    if (count < max_tabs) {
        tabs[count++] = TEST_CONFIG_TAB;
    }
    if (count < max_tabs) {
        tabs[count++] = TEST_HOTKEYS_TAB;
    }
    if (count < max_tabs) {
        tabs[count++] = TEST_RULES_TAB;
    }
    if (count < max_tabs) {
        tabs[count++] = TEST_PROJECTS_TAB;
    }
    return count;
}
void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}
int cofi_register_tab_provider(const CofiTabProvider *p) {
    (void)p;
    return 0;
}
int cofi_next_generation(int provider_id) { (void)provider_id; return -1; }
CofiActionStatus cofi_call_on_command_args(int provider_id, AppData *app, const char *args) {
    (void)provider_id; (void)app; (void)args; return COFI_NO_OP;
}

void show_help_commands(AppData *app) {
    (void)app;
}

void save_config(const CofiConfig *config) {
    (void)config;
}

void cofi_apply_disabled_providers(const char *disabled_ids) {
    (void)disabled_ids;
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

#define COMMAND_STUBS_EXCLUDE_UI 1
#include "command_handler_stubs.c"

void show_window(AppData *app) {
    (void)app;
    show_window_calls++;
}

void enter_command_mode(AppData *app) {
    (void)app;
}

void hide_window(AppData *app) { (void)app; }

gboolean detach_launch_shell(const char *command) { (void)command; return TRUE; }
void add_run_history_entry(RunMode *run_mode, const char *command) {
    (void)run_mode; (void)command;
}
gboolean extract_run_command(const char *entry_text, char *command_out, size_t command_size) {
    if (!entry_text || !command_out || command_size == 0) return FALSE;
    g_strlcpy(command_out, entry_text, command_size);
    return command_out[0] != '\0';
}

int get_active_window_id(Display *display) {
    (void)display;
    return 0;
}

void show_overlay(AppData *app, OverlayType type, void *data) {
    (void)app; (void)type; (void)data;
}

int replay_all_rules_against_open_windows(AppData *app) {
    (void)app;
    return 0;
}

gboolean replay_selected_filtered_rule(AppData *app) {
    (void)app;
    return TRUE;
}

#ifdef GTK_ENTRY
#undef GTK_ENTRY
#endif
#define GTK_ENTRY(widget) ((GtkEntry *)(widget))

#include "../src/rules_provider.c"
#include "../src/tab_switching.c"
#include "../src/command_handlers_ui.c"
#include "../src/daemon_socket_runtime.c"

static void reset_counters(void) {
    entry_clear_calls = 0;
    reset_selection_calls = 0;
    update_display_calls = 0;
    show_window_calls = 0;
    exit_command_mode_calls = 0;
    disabled_provider_tab = -1;
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->entry = (GtkWidget *)0x1;
    app->current_tab = TAB_WINDOWS;
}

static AppData make_app(void) {
    AppData app;
    init_app(&app);
    return app;
}

static AppData make_default_visibility_app(void) {
    AppData app;
    init_app(&app);

    for (int i = TAB_WINDOWS; i < COFI_MAX_TAB_HANDLES; i++) {
        app.tab_visibility[i] = TAB_VIS_HIDDEN;
    }
    app.tab_visibility[TAB_WINDOWS] = TAB_VIS_PINNED;
    app.tab_visibility[TEST_APPS_TAB] = TAB_VIS_PINNED;

    return app;
}

static void test_tab_switching_forward_cycles_all_tabs(void) {
    AppData app = make_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;

    TabMode expected[] = {
        TEST_WORKSPACES_TAB,
        TEST_HARPOON_TAB,
        TEST_APPS_TAB,
        TEST_NAMES_TAB,
        TEST_CONFIG_TAB,
        TEST_HOTKEYS_TAB,
        TEST_RULES_TAB,
        TEST_PROJECTS_TAB,
        TAB_WINDOWS
    };

    int expected_count = (int)(sizeof(expected) / sizeof(expected[0]));
    for (int i = 0; i < expected_count; i++) {
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
        TEST_PROJECTS_TAB,
        TEST_RULES_TAB,
        TEST_HOTKEYS_TAB,
        TEST_CONFIG_TAB,
        TEST_NAMES_TAB,
        TEST_APPS_TAB,
        TEST_HARPOON_TAB,
        TEST_WORKSPACES_TAB,
        TAB_WINDOWS
    };

    int expected_count = (int)(sizeof(expected) / sizeof(expected[0]));
    for (int i = 0; i < expected_count; i++) {
        gboolean handled = handle_tab_switching(&event, &app);
        ASSERT_TRUE("backward tab switch handled", handled == TRUE);

        char msg[64];
        snprintf(msg, sizeof(msg), "backward step %d reaches expected tab", i + 1);
        ASSERT_TRUE(msg, app.current_tab == expected[i]);
    }
}

static void test_switch_to_tab_updates_state_and_clears_entry(void) {
    AppData app = make_app();

    for (int tab = TAB_WINDOWS; tab < TAB_COUNT; tab++) {
        reset_counters();
        app.current_tab = (tab == TAB_WINDOWS) ? TEST_APPS_TAB : TAB_WINDOWS;

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
    ASSERT_TRUE("cmd_show names switches tab", app.current_tab == TEST_NAMES_TAB);
}

static void test_cmd_show_rules_switches_to_rules_tab(void) {
    AppData app = make_app();
    app.current_tab = TAB_WINDOWS;

    reset_counters();
    gboolean result = cmd_show(&app, NULL, "rules");

    ASSERT_TRUE("cmd_show rules returns FALSE", result == FALSE);
    ASSERT_TRUE("cmd_show rules exits command mode", exit_command_mode_calls == 1);
    ASSERT_TRUE("cmd_show rules switches tab", app.current_tab == TEST_RULES_TAB);
}

static void test_filter_rules_matches_pattern_and_commands(void) {
    AppData app = make_app();
    app.rules_config.count = 3;
    strcpy(app.rules_config.rules[0].pattern, "*term*");
    strcpy(app.rules_config.rules[0].commands, "sb on");
    strcpy(app.rules_config.rules[1].pattern, "*firefox*");
    strcpy(app.rules_config.rules[1].commands, "ew off");
    strcpy(app.rules_config.rules[2].pattern, "*mail*");
    strcpy(app.rules_config.rules[2].commands, "aot on");

    filter_rules(&app, "fire");
    ASSERT_TRUE("filter rules by pattern", app.filtered_rules_count == 1);
    ASSERT_TRUE("filter rules keeps index mapping", app.filtered_rule_indices[0] == 1);

    filter_rules(&app, "aot");
    ASSERT_TRUE("filter rules by commands", app.filtered_rules_count == 1);
    ASSERT_TRUE("filter rules command match uses same entry", app.filtered_rule_indices[0] == 2);
}

static void test_daemon_opcode_harpoon_switches_to_harpoon_tab(void) {
    AppData app = make_app();
    app.command_mode.state = CMD_MODE_NORMAL;

    reset_counters();
    daemon_socket_dispatch_opcode(&app, COFI_OPCODE_HARPOON);

    ASSERT_TRUE("daemon opcode harpoon shows window", show_window_calls == 1);
    ASSERT_TRUE("daemon opcode harpoon switches tab", app.current_tab == TEST_HARPOON_TAB);
}

static void test_surface_tab_surfaces_hidden_tab(void) {
    AppData app = make_default_visibility_app();

    ASSERT_TRUE("workspaces starts hidden", tab_is_visible(&app, TEST_WORKSPACES_TAB) == FALSE);

    surface_tab(&app, TEST_WORKSPACES_TAB);

    ASSERT_TRUE("surface_tab marks tab visible", tab_is_visible(&app, TEST_WORKSPACES_TAB) == TRUE);
    ASSERT_TRUE("surface_tab switches current tab", app.current_tab == TEST_WORKSPACES_TAB);
}

static void test_cmd_show_apps_resets_to_default_mode(void) {
    AppData app = make_app();
    app.current_tab = TEST_APPS_TAB;
    app.prefix_origin_tab = TAB_WINDOWS;
    app.apps_mode = APPS_MODE_PATH;

    reset_counters();
    cmd_show(&app, NULL, "apps");

    ASSERT_TRUE("cmd_show apps resets to DEFAULT mode", app.apps_mode == APPS_MODE_DEFAULT);
    ASSERT_TRUE("cmd_show apps switches tab", app.current_tab == TEST_APPS_TAB);
    ASSERT_TRUE("cmd_show apps records origin tab", app.prefix_origin_tab == TEST_APPS_TAB);
}

static void test_cmd_show_provider_records_origin_tab(void) {
    AppData app = make_app();
    app.current_tab = TAB_WINDOWS;

    reset_counters();
    cmd_show(&app, NULL, "workspaces");

    ASSERT_TRUE("cmd_show workspaces switches tab", app.current_tab == TEST_WORKSPACES_TAB);
    ASSERT_TRUE("cmd_show workspaces records origin tab",
                app.prefix_origin_tab == TAB_WINDOWS);
}

static void test_daemon_opcode_applications_resets_mode(void) {
    AppData app = make_app();
    app.current_tab = TEST_APPS_TAB;
    app.apps_mode = APPS_MODE_PATH;

    reset_counters();
    show_tab_for_opcode(&app, TEST_APPS_TAB);

    ASSERT_TRUE("daemon Applications opcode resets mode", app.apps_mode == APPS_MODE_DEFAULT);
}

static void test_tab_switching_skips_hidden_tabs(void) {
    AppData app = make_default_visibility_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;

    gboolean handled = handle_tab_switching(&event, &app);
    ASSERT_TRUE("tab switch handled with hidden tabs", handled == TRUE);
    ASSERT_TRUE("forward skips hidden to apps", app.current_tab == TEST_APPS_TAB);

    handled = handle_tab_switching(&event, &app);
    ASSERT_TRUE("tab switch wraps pinned tabs", handled == TRUE);
    ASSERT_TRUE("forward wraps back to windows", app.current_tab == TAB_WINDOWS);
}

static void test_show_all_tabs_cycles_hidden_tabs(void) {
    AppData app = make_default_visibility_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;
    app.config.show_all_tabs = 1;

    gboolean handled = handle_tab_switching(&event, &app);
    ASSERT_TRUE("show_all_tabs tab switch handled", handled == TRUE);
    ASSERT_TRUE("show_all_tabs includes hidden workspaces", app.current_tab == TEST_WORKSPACES_TAB);
    ASSERT_TRUE("hidden tab reports visible with show_all_tabs",
                tab_is_visible(&app, TEST_PROJECTS_TAB) == TRUE);
}

static void test_show_all_tabs_skips_unavailable_provider_tabs(void) {
    AppData app = make_default_visibility_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;
    app.config.show_all_tabs = 1;
    disabled_provider_tab = TEST_WORKSPACES_TAB;

    gboolean handled = handle_tab_switching(&event, &app);
    ASSERT_TRUE("show_all_tabs handles with unavailable provider", handled == TRUE);
    ASSERT_TRUE("show_all_tabs skips disabled workspaces provider", app.current_tab == TEST_HARPOON_TAB);
    ASSERT_TRUE("disabled provider tab reports invisible",
                tab_is_visible(&app, TEST_WORKSPACES_TAB) == FALSE);
}

static void test_tab_switching_clears_surfaced_tabs_on_pinned_return(void) {
    AppData app = make_default_visibility_app();
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Tab;
    event.state = GDK_SHIFT_MASK;
    disabled_provider_tab = -1;

    surface_tab(&app, TEST_WORKSPACES_TAB);
    ASSERT_TRUE("workspaces surfaced before cycling", app.tab_visibility[TEST_WORKSPACES_TAB] == TAB_VIS_SURFACED);

    gboolean handled = handle_tab_switching(&event, &app);
    ASSERT_TRUE("shift-tab handled from surfaced tab", handled == TRUE);
    ASSERT_TRUE("shift-tab moved to pinned windows", app.current_tab == TAB_WINDOWS);
    ASSERT_TRUE("surfaced tab hidden again after leaving", app.tab_visibility[TEST_WORKSPACES_TAB] == TAB_VIS_HIDDEN);
}

static void test_command_help_wrap_respects_width_budget(void) {
    char *help = generate_command_help_text(HELP_FORMAT_CLI, 115);
    ASSERT_TRUE("help text generated", help != NULL);
    if (!help) {
        return;
    }

    const char *line = help;
    gboolean all_fit = TRUE;
    while (*line) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        if (len > 115) {
            all_fit = FALSE;
            break;
        }
        if (!nl) {
            break;
        }
        line = nl + 1;
    }

    ASSERT_TRUE("all help lines fit within 115 columns", all_fit == TRUE);
    free(help);
}

static void test_command_help_wrap_show_continuation_alignment(void) {
    char *help = generate_command_help_text(HELP_FORMAT_CLI, 115);
    ASSERT_TRUE("help text generated", help != NULL);
    if (!help) {
        return;
    }

    const char *show_line = strstr(help, "  show [MODE]");
    ASSERT_TRUE("show line exists", show_line != NULL);
    if (!show_line) {
        free(help);
        return;
    }

    const char *next_line = strchr(show_line, '\n');
    ASSERT_TRUE("show continuation line exists", next_line != NULL);
    if (!next_line) {
        free(help);
        return;
    }
    next_line++;

    gboolean aligned = TRUE;
    for (int i = 0; i < 45; i++) {
        if (next_line[i] != ' ') {
            aligned = FALSE;
            break;
        }
    }
    ASSERT_TRUE("show continuation starts at description column (45)", aligned == TRUE);
    free(help);
}

static void test_command_help_wrap_prefers_word_boundaries(void) {
    char *help = generate_command_help_text(HELP_FORMAT_CLI, 115);
    ASSERT_TRUE("help text generated", help != NULL);
    if (!help) {
        return;
    }

    const char *show_line = strstr(help, "  show [MODE]");
    ASSERT_TRUE("show line exists", show_line != NULL);
    if (!show_line) {
        free(help);
        return;
    }

    const char *next_line = strchr(show_line, '\n');
    ASSERT_TRUE("show wraps to next line", next_line != NULL);
    if (!next_line) {
        free(help);
        return;
    }

    const char *cont = next_line + 45;
    gboolean not_mid_word = TRUE;
    if (next_line - 2 >= help && *cont != '\0') {
        unsigned char before = (unsigned char)*(next_line - 2);
        unsigned char after = (unsigned char)*cont;
        not_mid_word = !(isalnum(before) && isalnum(after));
    }

    ASSERT_TRUE("show wrap does not split a word token", not_mid_word == TRUE);
    free(help);
}

static void test_command_help_width_zero_is_unwrapped(void) {
    char *help = generate_command_help_text(HELP_FORMAT_CLI, 0);
    ASSERT_TRUE("help text generated", help != NULL);
    if (!help) {
        return;
    }

    ASSERT_TRUE("unwrapped show line remains single-line description",
                strstr(help,
                       "  show [MODE]                              - Show cofi in a specific mode (windows/command/run/workspaces/harpoon/names/config/rules/apps)\n")
                    != NULL);
    free(help);
}

int main(void) {
    printf("Tab visibility safety-net tests\n");
    printf("===============================\n\n");

    register_tab_visibility_commands();

    test_tab_switching_forward_cycles_all_tabs();
    test_tab_switching_backward_cycles_all_tabs();
    test_switch_to_tab_updates_state_and_clears_entry();
    test_cmd_show_names_switches_to_names_tab();
    test_cmd_show_rules_switches_to_rules_tab();
    test_filter_rules_matches_pattern_and_commands();
    test_daemon_opcode_harpoon_switches_to_harpoon_tab();
    test_surface_tab_surfaces_hidden_tab();
    test_cmd_show_apps_resets_to_default_mode();
    test_cmd_show_provider_records_origin_tab();
    test_daemon_opcode_applications_resets_mode();
    test_tab_switching_skips_hidden_tabs();
    test_show_all_tabs_cycles_hidden_tabs();
    test_show_all_tabs_skips_unavailable_provider_tabs();
    test_tab_switching_clears_surfaced_tabs_on_pinned_return();
    test_command_help_wrap_respects_width_budget();
    test_command_help_wrap_show_continuation_alignment();
    test_command_help_wrap_prefers_word_boundaries();
    test_command_help_width_zero_is_unwrapped();

    printf("\n===============================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
