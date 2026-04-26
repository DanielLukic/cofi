#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../src/app_data.h"
#include "../src/overlay_manager.h"
#include "../src/overlay_harpoon.h"
#include "../src/overlay_name.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_hide_overlay_calls;
static int g_update_display_calls;
static int g_save_harpoon_calls;
static int g_unassign_calls;
static int g_save_named_windows_calls;
static int g_delete_custom_name_calls;
static int g_filter_names_calls;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

GtkWidget *create_markup_label(const char *markup, gboolean use_markup) {
    GtkWidget *label = gtk_label_new(markup ? markup : "");
    if (use_markup) {
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    }
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
    if (!dest || dest_size <= 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, (size_t)dest_size - 1);
    dest[dest_size - 1] = '\0';
}

void hide_overlay(AppData *app) {
    g_hide_overlay_calls++;

    if (app->current_overlay == OVERLAY_HARPOON_DELETE) {
        app->harpoon_delete.pending_delete = FALSE;
        app->harpoon_delete.delete_slot = -1;
    }
    if (app->current_overlay == OVERLAY_NAME_DELETE) {
        app->name_delete.pending_delete = FALSE;
        app->name_delete.manager_index = -1;
        app->name_delete.custom_name[0] = '\0';
    }

    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

void save_harpoon_slots(const HarpoonManager *harpoon) {
    (void)harpoon;
    g_save_harpoon_calls++;
}

void unassign_slot(HarpoonManager *harpoon, int slot) {
    g_unassign_calls++;
    if (!harpoon || slot < 0 || slot >= MAX_HARPOON_SLOTS) {
        return;
    }
    memset(&harpoon->slots[slot], 0, sizeof(harpoon->slots[slot]));
}

void assign_custom_name(NamedWindowManager *manager, const WindowInfo *window, const char *custom_name) {
    (void)manager;
    (void)window;
    (void)custom_name;
}

void update_custom_name(NamedWindowManager *manager, int index, const char *new_name) {
    (void)manager;
    (void)index;
    (void)new_name;
}

int find_named_window_index(const NamedWindowManager *manager, Window id) {
    if (!manager) {
        return -1;
    }
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].id == id) {
            return i;
        }
    }
    return -1;
}

int find_named_window_by_name(const NamedWindowManager *manager, const char *custom_name) {
    if (!manager || !custom_name) {
        return -1;
    }
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->entries[i].custom_name, custom_name) == 0) {
            return i;
        }
    }
    return -1;
}

void delete_custom_name(NamedWindowManager *manager, int index) {
    g_delete_custom_name_calls++;
    if (!manager || index < 0 || index >= manager->count) {
        return;
    }

    for (int i = index; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    manager->count--;
}

void save_named_windows(const NamedWindowManager *manager) {
    (void)manager;
    g_save_named_windows_calls++;
}

void filter_windows(AppData *app, const char *query) {
    (void)app;
    (void)query;
}

void filter_names(AppData *app, const char *filter) {
    g_filter_names_calls++;
    (void)filter;
    app->filtered_names_count = app->names.count;
    for (int i = 0; i < app->names.count; i++) {
        app->filtered_names[i] = app->names.entries[i];
    }
}

static void reset_captures(void) {
    g_hide_overlay_calls = 0;
    g_update_display_calls = 0;
    g_save_harpoon_calls = 0;
    g_unassign_calls = 0;
    g_save_named_windows_calls = 0;
    g_delete_custom_name_calls = 0;
    g_filter_names_calls = 0;
}

static GdkEventKey make_key(guint keyval, GdkModifierType state) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = keyval;
    event.state = state;
    return event;
}

static void test_harpoon_delete_confirm_y_clears_state_and_hides_overlay(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = gtk_entry_new();
    app.current_tab = TAB_HARPOON;
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_HARPOON_DELETE;
    app.harpoon_delete.pending_delete = TRUE;
    app.harpoon_delete.delete_slot = 2;
    app.harpoon.slots[2].assigned = 1;

    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_y, 0);
    gboolean handled = handle_harpoon_delete_key_press(&app, &ev);

    ASSERT_TRUE("Harpoon delete Y handled", handled == TRUE);
    ASSERT_TRUE("Harpoon delete Y unassigns target slot", g_unassign_calls == 1 && app.harpoon.slots[2].assigned == 0);
    ASSERT_TRUE("Harpoon delete Y persists", g_save_harpoon_calls == 1);
    ASSERT_TRUE("Harpoon delete Y clears pending state", app.harpoon_delete.pending_delete == FALSE && app.harpoon_delete.delete_slot == -1);
    ASSERT_TRUE("Harpoon delete Y hides overlay + refreshes UI", g_hide_overlay_calls == 1 && g_update_display_calls == 1);
}

static void test_harpoon_delete_cancel_n_clears_state_and_hides_overlay(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = gtk_entry_new();
    app.current_tab = TAB_HARPOON;
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_HARPOON_DELETE;
    app.harpoon_delete.pending_delete = TRUE;
    app.harpoon_delete.delete_slot = 4;
    app.harpoon.slots[4].assigned = 1;

    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_n, 0);
    gboolean handled = handle_harpoon_delete_key_press(&app, &ev);

    ASSERT_TRUE("Harpoon delete N handled", handled == TRUE);
    ASSERT_TRUE("Harpoon delete N does not delete slot", g_unassign_calls == 0 && app.harpoon.slots[4].assigned == 1);
    ASSERT_TRUE("Harpoon delete N does not persist delete", g_save_harpoon_calls == 0);
    ASSERT_TRUE("Harpoon delete N clears pending state", app.harpoon_delete.pending_delete == FALSE && app.harpoon_delete.delete_slot == -1);
    ASSERT_TRUE("Harpoon delete N hides overlay + refreshes UI", g_hide_overlay_calls == 1 && g_update_display_calls == 1);
}

static void test_name_delete_confirm_y_deletes_and_clamps_last_row(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = gtk_entry_new();
    app.current_tab = TAB_NAMES;
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_NAME_DELETE;

    app.names.count = 3;
    strcpy(app.names.entries[0].custom_name, "alpha");
    app.names.entries[0].id = (Window)0xA;
    strcpy(app.names.entries[1].custom_name, "beta");
    app.names.entries[1].id = (Window)0xB;
    strcpy(app.names.entries[2].custom_name, "gamma");
    app.names.entries[2].id = (Window)0xC;

    app.name_delete.pending_delete = TRUE;
    app.name_delete.manager_index = 2;
    strcpy(app.name_delete.custom_name, "gamma");
    app.selection.names_index = 2;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "ga");

    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_y, 0);
    gboolean handled = handle_name_delete_key_press(&app, &ev);

    ASSERT_TRUE("Name delete Y handled", handled == TRUE);
    ASSERT_TRUE("Name delete Y removes target", app.names.count == 2 && strcmp(app.names.entries[1].custom_name, "beta") == 0);
    ASSERT_TRUE("Name delete Y persists + refilters", g_save_named_windows_calls == 1 && g_filter_names_calls == 1);
    ASSERT_TRUE("Name delete Y clamps last-row selection", app.selection.names_index == 1);
    ASSERT_TRUE("Name delete Y clears pending state", app.name_delete.pending_delete == FALSE && app.name_delete.manager_index == -1);
    ASSERT_TRUE("Name delete Y hides overlay + refreshes UI", g_hide_overlay_calls == 1 && g_update_display_calls == 1);
}

static void test_name_delete_confirm_ctrl_d_deletes_and_hides_overlay(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = gtk_entry_new();
    app.current_tab = TAB_NAMES;
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_NAME_DELETE;

    app.names.count = 2;
    strcpy(app.names.entries[0].custom_name, "alpha");
    app.names.entries[0].id = (Window)0xA;
    strcpy(app.names.entries[1].custom_name, "beta");
    app.names.entries[1].id = (Window)0xB;

    app.name_delete.pending_delete = TRUE;
    app.name_delete.manager_index = 0;
    strcpy(app.name_delete.custom_name, "alpha");

    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled = handle_name_delete_key_press(&app, &ev);

    ASSERT_TRUE("Name delete Ctrl+D handled", handled == TRUE);
    ASSERT_TRUE("Name delete Ctrl+D removes target", app.names.count == 1 && strcmp(app.names.entries[0].custom_name, "beta") == 0);
    ASSERT_TRUE("Name delete Ctrl+D persists + refilters", g_save_named_windows_calls == 1 && g_filter_names_calls == 1);
    ASSERT_TRUE("Name delete Ctrl+D hides overlay + refreshes UI", g_hide_overlay_calls == 1 && g_update_display_calls == 1);
}

static void test_name_delete_confirm_ctrl_d_works_for_orphan_fallback(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = gtk_entry_new();
    app.current_tab = TAB_NAMES;
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_NAME_DELETE;

    app.names.count = 1;
    strcpy(app.names.entries[0].custom_name, "orphan");
    app.names.entries[0].id = 0;

    app.name_delete.pending_delete = TRUE;
    app.name_delete.manager_index = -1;
    strcpy(app.name_delete.custom_name, "orphan");

    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    gboolean handled = handle_name_delete_key_press(&app, &ev);

    ASSERT_TRUE("Name delete Ctrl+D orphan handled", handled == TRUE);
    ASSERT_TRUE("Name delete Ctrl+D orphan deletes via fallback", app.names.count == 0 && g_delete_custom_name_calls == 1);
    ASSERT_TRUE("Name delete Ctrl+D orphan persists", g_save_named_windows_calls == 1);
    ASSERT_TRUE("Name delete Ctrl+D orphan hides overlay + refreshes UI", g_hide_overlay_calls == 1 && g_update_display_calls == 1);
}

static void test_name_delete_cancel_n_clears_state_and_hides_overlay(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = gtk_entry_new();
    app.current_tab = TAB_NAMES;
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_NAME_DELETE;

    app.names.count = 1;
    strcpy(app.names.entries[0].custom_name, "beta");
    app.name_delete.pending_delete = TRUE;
    app.name_delete.manager_index = 0;
    strcpy(app.name_delete.custom_name, "beta");

    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_n, 0);
    gboolean handled = handle_name_delete_key_press(&app, &ev);

    ASSERT_TRUE("Name delete N handled", handled == TRUE);
    ASSERT_TRUE("Name delete N does not delete", app.names.count == 1 && g_delete_custom_name_calls == 0);
    ASSERT_TRUE("Name delete N does not persist", g_save_named_windows_calls == 0);
    ASSERT_TRUE("Name delete N clears pending state", app.name_delete.pending_delete == FALSE && app.name_delete.manager_index == -1);
    ASSERT_TRUE("Name delete N hides overlay + refreshes UI", g_hide_overlay_calls == 1 && g_update_display_calls == 1);
}

static gboolean handle_overlay_escape_for_test(AppData *app, GdkEventKey *event) {
    if (!app->overlay_active) {
        return FALSE;
    }
    if (event->keyval == GDK_KEY_Escape) {
        hide_overlay(app);
        return TRUE;
    }
    return FALSE;
}

static void test_name_delete_cancel_esc_clears_state_via_overlay_manager(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.entry = gtk_entry_new();
    app.current_tab = TAB_NAMES;
    app.overlay_active = TRUE;
    app.current_overlay = OVERLAY_NAME_DELETE;

    app.names.count = 1;
    strcpy(app.names.entries[0].custom_name, "beta");
    app.name_delete.pending_delete = TRUE;
    app.name_delete.manager_index = 0;
    strcpy(app.name_delete.custom_name, "beta");

    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_Escape, 0);
    gboolean handled = handle_overlay_escape_for_test(&app, &ev);

    ASSERT_TRUE("Name delete Esc handled", handled == TRUE);
    ASSERT_TRUE("Name delete Esc does not delete", app.names.count == 1 && g_delete_custom_name_calls == 0);
    ASSERT_TRUE("Name delete Esc clears pending state", app.name_delete.pending_delete == FALSE && app.name_delete.manager_index == -1);
    ASSERT_TRUE("Name delete Esc hides overlay", g_hide_overlay_calls == 1 && app.overlay_active == FALSE && app.current_overlay == OVERLAY_NONE);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Overlay delete-flow tests\n");
        printf("=========================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    printf("Overlay delete-flow tests\n");
    printf("=========================\n\n");

    test_harpoon_delete_confirm_y_clears_state_and_hides_overlay();
    test_harpoon_delete_cancel_n_clears_state_and_hides_overlay();
    test_name_delete_confirm_y_deletes_and_clamps_last_row();
    test_name_delete_confirm_ctrl_d_deletes_and_hides_overlay();
    test_name_delete_confirm_ctrl_d_works_for_orphan_fallback();
    test_name_delete_cancel_n_clears_state_and_hides_overlay();
    test_name_delete_cancel_esc_clears_state_via_overlay_manager();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
