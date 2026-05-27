#include <stdio.h>
#include <string.h>
#include "core/app/app_data.h"
#include "matching/filter.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int mock_current_desktop = 0;

void update_history(AppData *app) { (void)app; }
void partition_and_reorder(AppData *app) { (void)app; }
int get_current_desktop(Display *display) { (void)display; return mock_current_desktop; }
void preserve_selection(AppData *app) { (void)app; }
void restore_selection(AppData *app) { (void)app; }
void validate_selection(AppData *app) { (void)app; }

#include "matching/filter.c"

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
    reset_app(&app);
    match_entry_manager_init(&app.matching);
    add_window(&app, 0x1, 0, "google-chrome", "Raw Window Title", "Google-chrome");
    match_entry_assign_custom_name(&app.matching, &app.history[0], "Alias");

    filter_windows(&app, "alias");

    ASSERT_TRUE("filter finds one window", app.filtered_count == 1);
    ASSERT_TRUE("history title remains raw",
                strcmp(app.history[0].title, "Raw Window Title") == 0);
}

static void test_compose_window_display_title_prefixes_custom_name(void) {
    MatchEntryManager manager;
    match_entry_manager_init(&manager);
    WindowInfo window = {0};
    window.id = 0x2;
    strncpy(window.title, "Window Title", sizeof(window.title) - 1);

    match_entry_assign_custom_name(&manager, &window, "Alias");

    char composed[MAX_TITLE_LEN];
    compose_window_display_title(&manager, &window, composed, sizeof(composed));
    ASSERT_TRUE("compose prefixes custom name",
                strcmp(composed, "Alias - Window Title") == 0);

    manager.entries[0].custom_name[0] = '\0';
    compose_window_display_title(&manager, &window, composed, sizeof(composed));
    ASSERT_TRUE("compose falls back to raw title",
                strcmp(composed, "Window Title") == 0);
}

int main(void) {
    test_filter_keeps_history_title_raw();
    test_compose_window_display_title_prefixes_custom_name();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
