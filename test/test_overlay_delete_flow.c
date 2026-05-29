#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "ui/overlay_confirm.h"
#include "harpoon/overlay_harpoon.h"
#include "names/overlay_names.h"
#include "sessions/overlay_sessions.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_hide_overlay_calls = 0;
static int g_update_display_calls = 0;
static int g_unassign_calls = 0;
static int g_save_harpoon_calls = 0;
static int g_save_names_calls = 0;
static int g_save_match_calls = 0;
static int g_delete_by_match_id_calls = 0;
static int g_deleted_session_calls = 0;
static int g_removed_session_calls = 0;

#define TEST_HARPOON_TAB ((TabMode)(TAB_COUNT + 1))
#define TEST_NAMES_TAB ((TabMode)(TAB_COUNT + 2))

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void show_overlay(AppData *app, OverlayType type, gpointer data) {
    (void)data;
    app->overlay_active = TRUE;
    app->current_overlay = type;
}

void hide_overlay(AppData *app) {
    g_hide_overlay_calls++;
    clear_confirm_overlay_state(app);
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;
}

GtkWidget *create_markup_label(const char *markup, gboolean use_markup) {
    GtkWidget *label = gtk_label_new(markup ? markup : "");
    if (use_markup) gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    return label;
}

GtkWidget *create_centered_label(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    return label;
}

void add_horizontal_separator(GtkWidget *parent) {
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent), sep, FALSE, FALSE, 0);
}

void safe_string_copy(char *dest, const char *src, int dest_size) {
    if (!dest || dest_size <= 0) return;
    g_strlcpy(dest, src ? src : "", (gsize)dest_size);
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

void unassign_slot(HarpoonManager *harpoon, int slot) {
    g_unassign_calls++;
    if (!harpoon || slot < 0 || slot >= MAX_HARPOON_SLOTS) return;
    memset(&harpoon->slots[slot], 0, sizeof(harpoon->slots[slot]));
}

void save_harpoon_slots(const HarpoonManager *harpoon) {
    (void)harpoon;
    g_save_harpoon_calls++;
}

int matching_run_gc(AppData *app) {
    (void)app;
    return 0;
}

void match_entry_delete_by_match_id(MatchEntryManager *manager, int match_id) {
    g_delete_by_match_id_calls++;
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
    for (int i = idx; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    memset(&manager->entries[manager->count - 1], 0, sizeof(manager->entries[0]));
    manager->count--;
}

void save_match_entries(const MatchEntryManager *manager) {
    (void)manager;
    g_save_match_calls++;
}

int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) {
    if (!manager) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].match_id == match_id) return i;
    }
    return -1;
}

void filter_windows(AppData *app, const char *query) {
    (void)app; (void)query;
}

bool names_assign_window(AppData *app, WindowInfo *window, const char *name) {
    (void)app; (void)window; (void)name;
    return true;
}

bool names_store_save(const NamesStore *store) {
    (void)store;
    g_save_names_calls++;
    return true;
}

bool names_store_set(NamesStore *store, int match_id, const char *name) {
    if (!store || store->count >= MAX_WINDOWS) return false;
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].match_id == match_id) {
            g_strlcpy(store->records[i].custom_name, name ? name : "",
                      sizeof(store->records[i].custom_name));
            return true;
        }
    }
    store->records[store->count].match_id = match_id;
    g_strlcpy(store->records[store->count].custom_name, name ? name : "",
              sizeof(store->records[store->count].custom_name));
    store->count++;
    return true;
}

NameRecord *names_store_find_by_custom_name(NamesStore *store, const char *name) {
    if (!store || !name) return NULL;
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->records[i].custom_name, name) == 0) return &store->records[i];
    }
    return NULL;
}

bool names_store_remove_by_match_id(NamesStore *store, int match_id) {
    if (!store) return false;
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].match_id != match_id) continue;
        for (int j = i; j < store->count - 1; j++) {
            store->records[j] = store->records[j + 1];
        }
        store->count--;
        return true;
    }
    return false;
}

NameRecord *names_selected_record(AppData *app) {
    if (!app || app->filtered_names_count <= 0) return NULL;
    return &app->filtered_names[0];
}

int names_selected_store_index(AppData *app) {
    (void)app;
    return 0;
}

void names_on_query_changed(AppData *app, const char *query) {
    (void)query;
    app->filtered_names_count = app->names.count;
    for (int i = 0; i < app->names.count; i++) {
        app->filtered_names[i] = app->names.records[i];
        app->filtered_names_indices[i] = i;
    }
}

void filter_harpoon(AppData *app, const char *query) {
    (void)query;
    app->filtered_harpoon_count = 0;
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (!app->harpoon.slots[i].assigned) continue;
        app->filtered_harpoon[app->filtered_harpoon_count] = app->harpoon.slots[i];
        app->filtered_harpoon_indices[app->filtered_harpoon_count] = i;
        app->filtered_harpoon_count++;
    }
}

void preserve_selection(AppData *app) {
    (void)app;
}

void restore_selection(AppData *app) {
    (void)app;
}

gboolean sessions_delete_path(const char *path) {
    (void)path;
    g_deleted_session_calls++;
    return TRUE;
}

void sessions_provider_remove_path(AppData *app, const char *path) {
    (void)app; (void)path;
    g_removed_session_calls++;
}

gboolean sessions_rename_result(const SessionResult *result, const char *new_name) {
    (void)result; (void)new_name;
    return TRUE;
}

void sessions_provider_rename_path(AppData *app, const char *path, const char *new_name) {
    (void)app; (void)path; (void)new_name;
}

TabMode harpoon_tab_mode(void) { return TEST_HARPOON_TAB; }
TabMode names_tab_mode(void) { return TEST_NAMES_TAB; }

static GdkEventKey key_confirm_y(void) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_y;
    return event;
}

static void reset_counters(void) {
    g_hide_overlay_calls = 0;
    g_update_display_calls = 0;
    g_unassign_calls = 0;
    g_save_harpoon_calls = 0;
    g_save_names_calls = 0;
    g_save_match_calls = 0;
    g_delete_by_match_id_calls = 0;
    g_deleted_session_calls = 0;
    g_removed_session_calls = 0;
}

static void test_harpoon_delete_confirm_flow(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.harpoon.slots[2].assigned = 1;
    app.harpoon.slots[2].match_id = 12;
    app.matching.count = 1;
    app.matching.entries[0].match_id = 12;
    strcpy(app.matching.entries[0].original_title, "Terminal");

    reset_counters();
    show_harpoon_delete_confirm(&app, 2);
    GdkEventKey ev = key_confirm_y();
    gboolean handled = handle_confirm_overlay_key_press(&app, &ev);

    ASSERT_TRUE("harpoon delete handled", handled == TRUE);
    ASSERT_TRUE("harpoon slot unassigned", g_unassign_calls == 1 && app.harpoon.slots[2].assigned == 0);
    ASSERT_TRUE("harpoon delete removes owned match entry",
                g_delete_by_match_id_calls == 1 &&
                app.matching.count == 0 &&
                match_entry_find_index_by_match_id(&app.matching, 12) == -1);
    ASSERT_TRUE("harpoon delete persists matching", g_save_match_calls == 1);
    ASSERT_TRUE("harpoon save called", g_save_harpoon_calls == 1);
    ASSERT_TRUE("harpoon rows rebuilt", app.filtered_harpoon_count == 0);
    ASSERT_TRUE("harpoon UI updated", g_update_display_calls == 1 && g_hide_overlay_calls == 1);
}

static void test_name_delete_confirm_flow(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = gtk_entry_new();
    app.selection.provider_index = 1;
    app.names.count = 2;
    app.names.records[0].match_id = 10;
    strcpy(app.names.records[0].custom_name, "alpha");
    app.names.records[1].match_id = 11;
    strcpy(app.names.records[1].custom_name, "beta");
    app.matching.count = 2;
    app.matching.entries[0].match_id = 10;
    strcpy(app.matching.entries[0].original_title, "Alpha");
    app.matching.entries[1].match_id = 11;
    strcpy(app.matching.entries[1].original_title, "Beta");

    reset_counters();
    show_name_delete_confirm(&app, "beta", 11);
    GdkEventKey ev = key_confirm_y();
    gboolean handled = handle_confirm_overlay_key_press(&app, &ev);

    ASSERT_TRUE("name delete handled", handled == TRUE);
    ASSERT_TRUE("name removed", app.names.count == 1 && strcmp(app.names.records[0].custom_name, "alpha") == 0);
    ASSERT_TRUE("name delete removes owned match entry",
                g_delete_by_match_id_calls == 1 &&
                app.matching.count == 1 &&
                match_entry_find_index_by_match_id(&app.matching, 11) == -1 &&
                match_entry_find_index_by_match_id(&app.matching, 10) >= 0);
    ASSERT_TRUE("name delete persists matching", g_save_match_calls == 1);
    ASSERT_TRUE("name delete persisted", g_save_names_calls == 1);
    ASSERT_TRUE("name delete updated UI", g_update_display_calls == 1 && g_hide_overlay_calls == 1);
}

static void test_session_delete_confirm_flow(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    reset_counters();
    show_session_delete_confirm(&app, "tmux", "abc", "/tmp/x");
    GdkEventKey ev = key_confirm_y();
    gboolean handled = handle_confirm_overlay_key_press(&app, &ev);

    ASSERT_TRUE("session delete handled", handled == TRUE);
    ASSERT_TRUE("session delete executed", g_deleted_session_calls == 1);
    ASSERT_TRUE("session row removed", g_removed_session_calls == 1);
    ASSERT_TRUE("session delete hides overlay", g_hide_overlay_calls == 1);
}

int main(void) {
    int argc = 0;
    char **argv = NULL;
    if (!gtk_init_check(&argc, &argv)) {
        printf("Overlay delete flow tests\n");
        printf("=========================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Overlay delete flow tests\n");
    printf("=========================\n\n");

    test_harpoon_delete_confirm_flow();
    test_name_delete_confirm_flow();
    test_session_delete_confirm_flow();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
