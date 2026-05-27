#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "ui/overlay_manager.h"
#include "matching/overlay_pattern.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#define TEST_MATCHING_TAB ((TabMode)(TAB_COUNT + 10))
#define TEST_RULES_TAB ((TabMode)(TAB_COUNT + 11))
#define TEST_GEOM_TAB ((TabMode)(TAB_COUNT + 12))
#define TEST_HARPOON_TAB ((TabMode)(TAB_COUNT + 13))

static int g_save_match_entries_calls = 0;
static int g_save_rules_config_calls = 0;
static int g_geom_sync_calls = 0;
static char g_geom_sync_patterns[4][MAX_TITLE_LEN];
static int g_show_confirm_calls = 0;
static char g_last_confirm_title[128];

void show_window(AppData *app) { (void)app; }
void regrab_hotkeys(AppData *app) { (void)app; }
void clear_confirm_overlay_state(AppData *app) { (void)app; }
void overlay_create_content(AppData *app, OverlayType type, gpointer data) {
    (void)app; (void)type; (void)data;
}
gboolean overlay_dispatch_key_press(AppData *app, GdkEventKey *event) {
    (void)app; (void)event; return FALSE;
}
void focus_edit_entry_delayed(AppData *app) { (void)app; }
void focus_name_entry_delayed(AppData *app) { (void)app; }

void show_confirm_overlay(AppData *app, const char *title, const char *info, void (*cb)(AppData *)) {
    (void)app; (void)info; (void)cb;
    g_show_confirm_calls++;
    g_strlcpy(g_last_confirm_title, title ? title : "", sizeof(g_last_confirm_title));
}
void filter_matching(AppData *app, const char *query) { (void)app; (void)query; }
void filter_rules(AppData *app, const char *query) { (void)app; (void)query; }
void geom_on_query_changed(AppData *app, const char *query) { (void)app; (void)query; }
void filter_harpoon(AppData *app, const char *query) { (void)app; (void)query; }
void update_display(AppData *app) { (void)app; }
void geom_rule_sync_for_pattern(AppData *app, const char *pattern) {
    (void)app;
    if (g_geom_sync_calls < 4) {
        g_strlcpy(g_geom_sync_patterns[g_geom_sync_calls], pattern ? pattern : "",
                  sizeof(g_geom_sync_patterns[g_geom_sync_calls]));
    }
    g_geom_sync_calls++;
}

TabMode matching_tab_mode(void) { return TEST_MATCHING_TAB; }
TabMode rules_tab_mode(void) { return TEST_RULES_TAB; }
TabMode geom_tab_mode(void) { return TEST_GEOM_TAB; }
TabMode harpoon_tab_mode(void) { return TEST_HARPOON_TAB; }

MatchEntry *matching_selected_entry(AppData *app) { (void)app; return NULL; }
Rule *rules_selected_rule(AppData *app) { (void)app; return NULL; }
HarpoonSlot *harpoon_selected_slot(AppData *app, int *actual_slot) {
    (void)app; if (actual_slot) *actual_slot = -1; return NULL;
}

int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) {
    if (!manager || match_id <= 0) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].match_id == match_id) return i;
    }
    return -1;
}

void save_match_entries(const MatchEntryManager *manager) {
    (void)manager;
    g_save_match_entries_calls++;
}

int save_rules_config(const RulesConfig *config, const MatchEntryManager *manager) {
    (void)config;
    (void)manager;
    g_save_rules_config_calls++;
    return 1;
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->main_overlay = gtk_overlay_new();
    app->entry = gtk_entry_new();
    app->dialog_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    app->modal_background = gtk_event_box_new();
    app->window_visible = TRUE;
}

static GdkEventKey enter_event(void) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Return;
    return event;
}

static void test_pattern_overlay_cleanup_resets_target_match_id(void) {
    AppData app;
    init_app(&app);
    init_overlay_system(&app);

    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_MATCH_PATTERN_EDIT;
    app.pattern_edit.target_match_id = 42;

    hide_overlay(&app);

    ASSERT_TRUE("pattern target match id reset", app.pattern_edit.target_match_id == 0);
}

static void test_matching_pattern_edit_refreshes_rules_cache_when_referenced(void) {
    AppData app;
    init_app(&app);
    app.current_tab = matching_tab_mode();
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_MATCH_PATTERN_EDIT;

    MatchEntry *entry = &app.matching.entries[0];
    entry->match_id = 101;
    g_strlcpy(entry->original_title, "Old Title", sizeof(entry->original_title));
    g_strlcpy(entry->class_name, "Chromium", sizeof(entry->class_name));
    g_strlcpy(entry->instance, "chromium", sizeof(entry->instance));
    g_strlcpy(entry->type, "Normal", sizeof(entry->type));
    app.matching.count = 1;
    app.pattern_edit.target_match_id = 101;

    Rule *rule = &app.rules_config.rules[0];
    rule->match_id = 101;
    g_strlcpy(rule->pattern, "Old Title", sizeof(rule->pattern));
    app.rules_config.count = 1;

    create_pattern_edit_overlay_content(app.dialog_container, &app);
    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app.dialog_container), "name_entry");
    ASSERT_TRUE("pattern edit entry exists", name_entry != NULL);
    gtk_entry_set_text(GTK_ENTRY(name_entry), "New Title");

    g_save_match_entries_calls = 0;
    g_save_rules_config_calls = 0;
    GdkEventKey ev = enter_event();
    gboolean handled = handle_pattern_edit_key_press(&app, &ev);

    ASSERT_TRUE("pattern edit handled", handled == TRUE);
    ASSERT_TRUE("match entry updated", strcmp(app.matching.entries[0].original_title, "New Title") == 0);
    ASSERT_TRUE("matching save called", g_save_match_entries_calls == 1);
    ASSERT_TRUE("rules save called when referenced", g_save_rules_config_calls == 1);
}

static void test_matching_pattern_edit_triggers_geom_sync_when_layout_references_entry(void) {
    AppData app;
    init_app(&app);
    app.current_tab = matching_tab_mode();
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_MATCH_PATTERN_EDIT;

    MatchEntry *entry = &app.matching.entries[0];
    entry->match_id = 202;
    g_strlcpy(entry->original_title, "Old Geom", sizeof(entry->original_title));
    g_strlcpy(entry->class_name, "Chromium", sizeof(entry->class_name));
    g_strlcpy(entry->instance, "chromium", sizeof(entry->instance));
    g_strlcpy(entry->type, "Normal", sizeof(entry->type));
    app.matching.count = 1;
    app.pattern_edit.target_match_id = 202;

    app.layouts.count = 1;
    app.layouts.records[0].match_id = 202;

    create_pattern_edit_overlay_content(app.dialog_container, &app);
    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app.dialog_container), "name_entry");
    ASSERT_TRUE("geom ref pattern edit entry exists", name_entry != NULL);
    gtk_entry_set_text(GTK_ENTRY(name_entry), "New Geom");

    g_save_match_entries_calls = 0;
    g_save_rules_config_calls = 0;
    g_geom_sync_calls = 0;
    memset(g_geom_sync_patterns, 0, sizeof(g_geom_sync_patterns));

    GdkEventKey ev = enter_event();
    gboolean handled = handle_pattern_edit_key_press(&app, &ev);

    ASSERT_TRUE("geom ref pattern edit handled", handled == TRUE);
    ASSERT_TRUE("geom ref match entry updated", strcmp(app.matching.entries[0].original_title, "New Geom") == 0);
    ASSERT_TRUE("geom ref matching save called", g_save_match_entries_calls == 1);
    ASSERT_TRUE("geom ref rules not saved when no rule reference", g_save_rules_config_calls == 0);
    ASSERT_TRUE("geom ref sync called old+new", g_geom_sync_calls == 2);
    ASSERT_TRUE("geom ref first sync old pattern", strcmp(g_geom_sync_patterns[0], "Old Geom") == 0);
    ASSERT_TRUE("geom ref second sync new pattern", strcmp(g_geom_sync_patterns[1], "New Geom") == 0);
}

static void test_pattern_edit_keeps_user_wildcards_raw(void) {
    AppData app;
    init_app(&app);
    app.current_tab = matching_tab_mode();
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_MATCH_PATTERN_EDIT;

    MatchEntry *entry = &app.matching.entries[0];
    entry->match_id = 303;
    g_strlcpy(entry->original_title, "Old", sizeof(entry->original_title));
    g_strlcpy(entry->class_name, "Chromium", sizeof(entry->class_name));
    g_strlcpy(entry->instance, "chromium", sizeof(entry->instance));
    g_strlcpy(entry->type, "Normal", sizeof(entry->type));
    app.matching.count = 1;
    app.pattern_edit.target_match_id = 303;

    create_pattern_edit_overlay_content(app.dialog_container, &app);
    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app.dialog_container), "name_entry");
    ASSERT_TRUE("raw wildcard edit entry exists", name_entry != NULL);
    gtk_entry_set_text(GTK_ENTRY(name_entry), "foo*bar");

    GdkEventKey ev = enter_event();
    gboolean handled = handle_pattern_edit_key_press(&app, &ev);

    ASSERT_TRUE("raw wildcard edit handled", handled == TRUE);
    ASSERT_TRUE("raw wildcard kept unchanged", strcmp(app.matching.entries[0].original_title, "foo*bar") == 0);
}

static void test_orphan_pattern_edit_shows_feedback(void) {
    AppData app;
    init_app(&app);
    app.current_tab = rules_tab_mode();
    g_show_confirm_calls = 0;
    g_last_confirm_title[0] = '\0';

    gboolean shown = show_pattern_edit_overlay(&app, 0, "Commands: rl");
    ASSERT_TRUE("orphan pattern edit returns false", shown == FALSE);
    ASSERT_TRUE("orphan pattern edit shows confirm", g_show_confirm_calls == 1);
    ASSERT_TRUE("orphan pattern title", strcmp(g_last_confirm_title, "Cannot Edit Pattern") == 0);
}

static void test_show_pattern_overlay_stores_context_line(void) {
    AppData app;
    init_app(&app);
    app.current_tab = matching_tab_mode();
    app.matching.count = 1;
    app.matching.entries[0].match_id = 77;
    g_strlcpy(app.matching.entries[0].original_title, "Title",
              sizeof(app.matching.entries[0].original_title));

    gboolean shown = show_pattern_edit_overlay(&app, 77, "Slot: 1");
    ASSERT_TRUE("show pattern with context succeeds", shown == TRUE);
    ASSERT_TRUE("context line stored", strcmp(app.pattern_edit.context_line, "Slot: 1") == 0);
}

static void test_show_pattern_overlay_accepts_null_context(void) {
    AppData app;
    init_app(&app);
    app.current_tab = matching_tab_mode();
    app.matching.count = 1;
    app.matching.entries[0].match_id = 88;
    g_strlcpy(app.matching.entries[0].original_title, "Title",
              sizeof(app.matching.entries[0].original_title));
    g_strlcpy(app.pattern_edit.context_line, "stale", sizeof(app.pattern_edit.context_line));

    gboolean shown = show_pattern_edit_overlay(&app, 88, NULL);
    ASSERT_TRUE("show pattern with null context succeeds", shown == TRUE);
    ASSERT_TRUE("null context clears stored line", app.pattern_edit.context_line[0] == '\0');
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Overlay pattern tests\n");
        printf("=====================\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Overlay pattern tests\n");
    printf("=====================\n");

    test_pattern_overlay_cleanup_resets_target_match_id();
    test_matching_pattern_edit_refreshes_rules_cache_when_referenced();
    test_matching_pattern_edit_triggers_geom_sync_when_layout_references_entry();
    test_pattern_edit_keeps_user_wildcards_raw();
    test_orphan_pattern_edit_shows_feedback();
    test_show_pattern_overlay_stores_context_line();
    test_show_pattern_overlay_accepts_null_context();

    printf("\nResults: %d/%d passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
