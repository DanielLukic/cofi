#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"

static int pass = 0;
static int fail = 0;
static int g_reset_selection_calls = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

void reset_selection(AppData *app) { (void)app; g_reset_selection_calls++; }

void cofi_init_provider_defaults(CofiTabProvider *provider) {
    memset(provider, 0, sizeof(*provider));
    provider->hidden_by_default = 1;
    provider->modal_policy = COFI_MODAL_HIDE_ON_ESC;
}

int cofi_register_tab_provider(const CofiTabProvider *provider) {
    (void)provider;
    return 0;
}

int has_match(const char *pattern, const char *text) {
    if (!pattern || !*pattern) return 1;
    return text && strstr(text, pattern) != NULL;
}

void build_config_entries(const CofiConfig *config, ConfigEntry *entries, int *count) {
    (void)config;
    *count = 3;
    g_strlcpy(entries[0].key, "close_on_focus_loss", sizeof(entries[0].key));
    g_strlcpy(entries[0].value, "true", sizeof(entries[0].value));
    entries[0].type = CONFIG_TYPE_BOOL;

    g_strlcpy(entries[1].key, "digit_slot_mode", sizeof(entries[1].key));
    g_strlcpy(entries[1].value, "default", sizeof(entries[1].value));
    entries[1].type = CONFIG_TYPE_ENUM;

    g_strlcpy(entries[2].key, "tile_columns", sizeof(entries[2].key));
    g_strlcpy(entries[2].value, "3", sizeof(entries[2].value));
    entries[2].type = CONFIG_TYPE_INT;
}

#include "../src/config_provider.c"

static void test_filter_and_format_row(void) {
    AppData app = {0};
    CofiRowCells row = {0};

    filter_config(&app, "digit");
    ASSERT_TRUE("filter config narrows to one entry", app.filtered_config_count == 1);
    ASSERT_TRUE("filter config preserves matching key",
                strcmp(app.filtered_config[0].key, "digit_slot_mode") == 0);

    config_format_row(&app, 0, &row);
    ASSERT_TRUE("config row has key/value cells", row.cell_count == 2);
    ASSERT_TRUE("config row key text", strcmp(row.cells[0].text, "digit_slot_mode") == 0);
    ASSERT_TRUE("config row value text", strcmp(row.cells[1].text, "default") == 0);
    ASSERT_TRUE("config row actionable", row.row_flags == COFI_ROW_ACTIONABLE);
    ASSERT_TRUE("config match string includes key",
                strstr(config_match_string(&app, 0), "digit_slot_mode") != NULL);
    ASSERT_TRUE("config identity includes key",
                strcmp(config_row_identity(&app, 0), "config:digit_slot_mode") == 0);
}

static void test_empty_row_and_hint(void) {
    AppData app = {0};
    CofiRowCells row = {0};

    filter_config(&app, "missing");
    ASSERT_TRUE("empty config provider still has status row", config_row_count(&app) == 1);
    config_format_row(&app, 0, &row);
    ASSERT_TRUE("empty config row is status", row.cell_count == 1);
    ASSERT_TRUE("empty config row is not actionable", row.row_flags == 0);
    ASSERT_TRUE("empty config row text",
                strcmp(row.cells[0].text, "No matching config options found") == 0);
}

static void test_query_changed_resets_selection(void) {
    AppData app = {0};

    g_reset_selection_calls = 0;
    config_on_query_changed(&app, "tile");
    ASSERT_TRUE("query change filters config", app.filtered_config_count == 1);
    ASSERT_TRUE("query change resets selection", g_reset_selection_calls == 1);
}

static void test_selected_entry_clamps_and_select_key(void) {
    AppData app = {0};
    filter_config(&app, "");

    app.selection.provider_index = 99;
    ConfigEntry *entry = config_selected_entry(&app);
    ASSERT_TRUE("selected config clamps to last", entry != NULL);
    ASSERT_TRUE("selected config clamp index", app.selection.provider_index == 2);
    ASSERT_TRUE("selected config key", strcmp(entry->key, "tile_columns") == 0);

    config_select_key(&app, "digit_slot_mode");
    ASSERT_TRUE("select config key sets provider index", app.selection.provider_index == 1);

    config_select_key(&app, "missing");
    ASSERT_TRUE("select missing config key falls back to zero", app.selection.provider_index == 0);
}

int main(void) {
    printf("Config provider tests\n");
    printf("=====================\n\n");

    test_filter_and_format_row();
    test_empty_row_and_hint();
    test_query_changed_resets_selection();
    test_selected_entry_clamps_and_select_key();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
