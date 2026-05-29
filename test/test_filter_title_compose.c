#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "core/app/app_data.h"
#include "ui/window_filter.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int mock_current_desktop = 0;

static void set_test_home(const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-filter-compose-%ld-%s", (long)getpid(), name);
    mkdir(path, 0755);
    setenv("HOME", path, 1);
}

void update_history(AppData *app) { (void)app; }
void partition_and_reorder(AppData *app) { (void)app; }
int get_current_desktop(Display *display) { (void)display; return mock_current_desktop; }
void preserve_selection(AppData *app) { (void)app; }
void restore_selection(AppData *app) { (void)app; }
void validate_selection(AppData *app) { (void)app; }
void save_match_entries(const MatchEntryManager *manager) { (void)manager; }

#include "ui/window_filter.c"

static void reset_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = TAB_WINDOWS;
}

static void add_window(AppData *app, Window id, int desktop, const char *instance,
                       const char *title, const char *class_name) {
    int i = app->history_count;
    app->history[i].id = id;
    app->history[i].desktop = desktop;
    strncpy(app->history[i].instance, instance, sizeof(app->history[i].instance) - 1);
    strncpy(app->history[i].title, title, sizeof(app->history[i].title) - 1);
    strncpy(app->history[i].class_name, class_name, sizeof(app->history[i].class_name) - 1);
    strncpy(app->history[i].type, "Normal", sizeof(app->history[i].type) - 1);
    app->history_count++;
}

static void test_filter_keeps_history_title_raw(void) {
    AppData app;
    set_test_home("filter");
    reset_app(&app);
    match_entry_manager_init(&app.matching);
    names_store_init_with_path(&app.names, "/tmp/cofi-filter-names.json");
    add_window(&app, 0x1, 0, "google-chrome", "Raw Window Title", "Google-chrome");
    names_assign_window(&app, &app.history[0], "Alias");

    filter_windows(&app, "alias");

    ASSERT_TRUE("filter finds one window", app.filtered_count == 1);
    ASSERT_TRUE("history title remains raw",
                strcmp(app.history[0].title, "Raw Window Title") == 0);
}

static void test_compose_window_display_title_prefixes_custom_name(void) {
    MatchEntryManager manager;
    NamesStore names;
    match_entry_manager_init(&manager);
    names_store_init_with_path(&names, "/tmp/cofi-compose-names.json");
    WindowInfo window = {0};
    window.id = 0x2;
    strncpy(window.title, "Window Title", sizeof(window.title) - 1);
    strncpy(window.type, "Normal", sizeof(window.type) - 1);

    int match_id = matching_create_entry(&manager, &window);
    names_store_set(&names, match_id, "Alias");

    char composed[MAX_TITLE_LEN];
    compose_window_display_title(&manager, &names, &window, composed, sizeof(composed));
    ASSERT_TRUE("compose prefixes custom name",
                strcmp(composed, "Alias - Window Title") == 0);

    names_store_remove_by_match_id(&names, match_id);
    compose_window_display_title(&manager, &names, &window, composed, sizeof(composed));
    ASSERT_TRUE("compose falls back to raw title",
                strcmp(composed, "Window Title") == 0);
}

int main(void) {
    test_filter_keeps_history_title_raw();
    test_compose_window_display_title_prefixes_custom_name();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
