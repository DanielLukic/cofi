#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "commands/command_registry.h"
#include "ui/overlay_confirm.h"
#include "commands/core_commands.h"
#include "rules/overlay_rules.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_save_rules_calls;
static int g_hide_overlay_calls;
static int g_update_display_calls;

#define TEST_RULES_TAB ((TabMode)(TAB_COUNT + 1))

void hide_overlay(AppData *app) {
    clear_confirm_overlay_state(app);
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;
    g_hide_overlay_calls++;
}

void show_overlay(AppData *app, OverlayType type, gpointer data) {
    (void)data;
    app->overlay_active = TRUE;
    app->current_overlay = type;
}

int add_rule(RulesConfig *config, const char *pattern, const char *commands) {
    if (!config || !pattern || !commands || config->count >= MAX_RULES) return 0;
    g_strlcpy(config->rules[config->count].pattern, pattern, sizeof(config->rules[config->count].pattern));
    g_strlcpy(config->rules[config->count].commands, commands, sizeof(config->rules[config->count].commands));
    config->rules[config->count].match_id = 1;
    config->count++;
    return 1;
}

int remove_rule(RulesConfig *config, int index) {
    if (!config || index < 0 || index >= config->count) return 0;
    for (int i = index; i < config->count - 1; i++) {
        config->rules[i] = config->rules[i + 1];
    }
    config->count--;
    return 1;
}

int save_rules_config(const RulesConfig *config, const MatchEntryManager *manager) {
    (void)config;
    (void)manager;
    g_save_rules_calls++;
    return 1;
}

int matching_find_or_create_pattern_entry(MatchEntryManager *mgr, const char *pattern) {
    (void)mgr;
    (void)pattern;
    return 1;
}

void save_match_entries(const MatchEntryManager *manager) {
    (void)manager;
}

void filter_rules(AppData *app, const char *filter) {
    app->filtered_rules_count = 0;
    for (int i = 0; i < app->rules_config.count; i++) {
        if (filter && *filter && strstr(app->rules_config.rules[i].pattern, filter) == NULL &&
            strstr(app->rules_config.rules[i].commands, filter) == NULL) {
            continue;
        }
        app->filtered_rules[app->filtered_rules_count] = app->rules_config.rules[i];
        app->filtered_rule_indices[app->filtered_rules_count] = i;
        app->filtered_rules_count++;
    }
}

void validate_selection(AppData *app) {
    if (app->current_tab == TEST_RULES_TAB && app->filtered_rules_count > 0 &&
        app->selection.provider_index >= app->filtered_rules_count) {
        app->selection.provider_index = app->filtered_rules_count - 1;
    }
}

void update_scroll_position(AppData *app) { (void)app; }
void update_display(AppData *app) { (void)app; g_update_display_calls++; }
TabMode rules_tab_mode(void) { return TEST_RULES_TAB; }

Rule *rules_selected_rule(AppData *app) {
    if (!app || app->filtered_rules_count <= 0) return NULL;
    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_rules_count) idx = app->filtered_rules_count - 1;
    app->selection.provider_index = idx;
    return &app->filtered_rules[idx];
}

int rules_selected_config_index(AppData *app) {
    if (!rules_selected_rule(app)) return -1;
    return app->filtered_rule_indices[app->selection.provider_index];
}

void rules_select_config_index(AppData *app, int config_index) {
    (void)app;
    (void)config_index;
}

static GdkEventKey enter_event(void) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Return;
    return event;
}

static GdkEventKey confirm_event(void) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_y;
    return event;
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = TEST_RULES_TAB;
    app->dialog_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    app->entry = gtk_entry_new();
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
}

static void test_add_rule_persists_only(void) {
    AppData app;
    init_app(&app);
    g_save_rules_calls = g_hide_overlay_calls = g_update_display_calls = 0;

    create_rule_add_overlay_content(app.dialog_container, &app);
    GtkWidget *pattern = g_object_get_data(G_OBJECT(app.dialog_container), "rule_pattern_entry");
    GtkWidget *commands = g_object_get_data(G_OBJECT(app.dialog_container), "rule_commands_entry");
    gtk_entry_set_text(GTK_ENTRY(pattern), "*term*");
    gtk_entry_set_text(GTK_ENTRY(commands), "sb on");

    GdkEventKey ev = enter_event();
    gboolean handled = handle_rule_add_key_press(&app, &ev);

    ASSERT_TRUE("rule add handled", handled == TRUE);
    ASSERT_TRUE("rule add updates config", app.rules_config.count == 1);
    ASSERT_TRUE("rule add persisted", g_save_rules_calls == 1);
    ASSERT_TRUE("rule add hides overlay", g_hide_overlay_calls == 1);
    ASSERT_TRUE("rule add refreshes display", g_update_display_calls == 1);
}

static void test_add_rule_rejects_invalid_command(void) {
    AppData app;
    init_app(&app);
    g_save_rules_calls = g_hide_overlay_calls = 0;

    create_rule_add_overlay_content(app.dialog_container, &app);
    GtkWidget *pattern = g_object_get_data(G_OBJECT(app.dialog_container), "rule_pattern_entry");
    GtkWidget *commands = g_object_get_data(G_OBJECT(app.dialog_container), "rule_commands_entry");
    gtk_entry_set_text(GTK_ENTRY(pattern), "*term*");
    gtk_entry_set_text(GTK_ENTRY(commands), "definitely-not-a-command");

    GdkEventKey ev = enter_event();
    gboolean handled = handle_rule_add_key_press(&app, &ev);

    ASSERT_TRUE("invalid rule command handled", handled == TRUE);
    ASSERT_TRUE("invalid rule command not saved", app.rules_config.count == 0 && g_save_rules_calls == 0);
    ASSERT_TRUE("invalid rule command keeps overlay open", g_hide_overlay_calls == 0);
}

static void test_edit_rule_updates_entry(void) {
    AppData app;
    init_app(&app);
    g_save_rules_calls = g_hide_overlay_calls = 0;

    app.rules_config.count = 1;
    strcpy(app.rules_config.rules[0].pattern, "*term*");
    strcpy(app.rules_config.rules[0].commands, "sb on");
    app.rules_config.rules[0].match_id = 42;
    app.filtered_rules_count = 1;
    app.filtered_rule_indices[0] = 0;
    app.selection.provider_index = 0;

    create_rule_edit_overlay_content(app.dialog_container, &app);
    GtkWidget *pattern = g_object_get_data(G_OBJECT(app.dialog_container), "rule_pattern_entry");
    GtkWidget *commands = g_object_get_data(G_OBJECT(app.dialog_container), "rule_commands_entry");
    ASSERT_TRUE("rule edit has no pattern field", pattern == NULL);
    gtk_entry_set_text(GTK_ENTRY(commands), "ew off");

    GdkEventKey ev = enter_event();
    gboolean handled = handle_rule_edit_key_press(&app, &ev);

    ASSERT_TRUE("rule edit handled", handled == TRUE);
    ASSERT_TRUE("rule edit updated command", strcmp(app.rules_config.rules[0].commands, "ew off") == 0);
    ASSERT_TRUE("rule edit keeps pattern", strcmp(app.rules_config.rules[0].pattern, "*term*") == 0);
    ASSERT_TRUE("rule edit keeps match_id", app.rules_config.rules[0].match_id == 42);
    ASSERT_TRUE("rule edit persisted", g_save_rules_calls == 1);
}

static void test_delete_rule_clamps_selection(void) {
    AppData app;
    init_app(&app);
    g_save_rules_calls = 0;

    app.rules_config.count = 2;
    strcpy(app.rules_config.rules[0].pattern, "*a*");
    strcpy(app.rules_config.rules[0].commands, "sb on");
    strcpy(app.rules_config.rules[1].pattern, "*b*");
    strcpy(app.rules_config.rules[1].commands, "ew on");

    app.filtered_rules_count = 2;
    app.filtered_rule_indices[0] = 0;
    app.filtered_rule_indices[1] = 1;
    app.selection.provider_index = 1;
    show_rule_delete_confirm(&app, 1);

    GdkEventKey ev = confirm_event();
    gboolean handled = handle_confirm_overlay_key_press(&app, &ev);

    ASSERT_TRUE("rule delete handled", handled == TRUE);
    ASSERT_TRUE("rule delete reduced count", app.rules_config.count == 1);
    ASSERT_TRUE("rule delete clamped selection", app.selection.provider_index == 0);
    ASSERT_TRUE("rule delete persisted", g_save_rules_calls == 1);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Overlay rules tests\n");
        printf("===================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Overlay rules tests\n");
    printf("===================\n\n");

    cofi_command_registry_reset();
    cofi_register_core_commands();

    test_add_rule_persists_only();
    test_add_rule_rejects_invalid_command();
    test_edit_rule_updates_entry();
    test_delete_rule_clamps_selection();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
