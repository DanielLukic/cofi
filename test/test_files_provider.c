#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "commands/command_registry.h"
#include "config/config.h"
#include "providers/cofi_tab_provider.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

void exit_command_mode(AppData *app) {
    (void)app;
}

void surface_tab(AppData *app, TabMode tab) {
    if (app) app->current_tab = tab;
}

void update_display(AppData *app) {
    (void)app;
}

void reset_selection(AppData *app) {
    (void)app;
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

void log_set_level(int level) {
    (void)level;
}

void files_on_enter(AppData *app) {
    (void)app;
}

void files_on_leave(AppData *app) {
    (void)app;
}

void files_on_query_changed(AppData *app, const char *query) {
    (void)app;
    (void)query;
}

void files_search_refresh(AppData *app) {
    (void)app;
}

int files_row_count(AppData *app) {
    (void)app;
    return 0;
}

const char *files_path_at_visible(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return NULL;
}

const char *files_match_string(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

const char *files_row_identity(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

const char *files_status_message(AppData *app) {
    (void)app;
    return "";
}

gboolean files_status_is_error(AppData *app) {
    (void)app;
    return FALSE;
}

#include "files/files_provider.c"

static void reset_provider_state(void) {
    cofi_registry_reset();
    cofi_command_registry_reset();
    cofi_config_registry_reset();
    files_provider_register();
}

static void test_files_provider_registers_command_and_config(void) {
    reset_provider_state();

    ASSERT_TRUE("files provider registered",
                cofi_get_provider_id("files") >= 0);
    ASSERT_TRUE("files command registered",
                cofi_command_for_token("files") != NULL);
    ASSERT_TRUE("files.enabled config registered",
                cofi_config_entry_for_key("files.enabled") != NULL);
    ASSERT_TRUE("files.fd_path config registered",
                cofi_config_entry_for_key("files.fd_path") != NULL);
    ASSERT_TRUE("files.excludes config registered",
                cofi_config_entry_for_key("files.excludes") != NULL);
}

static void test_files_defaults_are_enabled_with_empty_overrides(void) {
    CofiConfig config;
    init_config_defaults(&config);

    ASSERT_TRUE("files.enabled default true", config.files_enabled == 1);
    ASSERT_TRUE("files.fd_path default empty", config.files_fd_path[0] == '\0');
    ASSERT_TRUE("files.excludes default empty", config.files_excludes[0] == '\0');
}

int main(void) {
    printf("files_provider tests\n");
    printf("====================\n\n");

    test_files_provider_registers_command_and_config();
    test_files_defaults_are_enabled_with_empty_overrides();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
