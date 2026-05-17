#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"

static int s_tests_run = 0;
static int s_tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        s_tests_run++; \
        if (cond) { \
            s_tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s\n", msg); \
        } \
    } while (0)

#define ASSERT_EQ(msg, a, b)     ASSERT_TRUE(msg, (a) == (b))
#define ASSERT_STR_EQ(msg, a, b) ASSERT_TRUE(msg, strcmp((a), (b)) == 0)
#define ASSERT_NULL(msg, p)      ASSERT_TRUE(msg, (p) == NULL)
#define ASSERT_NOT_NULL(msg, p)  ASSERT_TRUE(msg, (p) != NULL)

static int mock_row_count_5(AppData *app) { (void)app; return 5; }
static int mock_row_count_3(AppData *app) { (void)app; return 3; }

static CofiActionStatus mock_enter_pressed(AppData *app, int fi, int ri,
                                            const char *text, int modifier_state) {
    (void)app; (void)fi; (void)ri; (void)text; (void)modifier_state;
    return COFI_HANDLED_KEEP;
}

static CofiActionStatus mock_command_args(AppData *app, const char *args) {
    (void)app; (void)args;
    return COFI_HANDLED_KEEP;
}

static void test_init_defaults(void) {
    CofiTabProvider p;
    memset(&p, 0xFF, sizeof(p));
    cofi_init_provider_defaults(&p);

    ASSERT_EQ("init: hidden_by_default=1", p.hidden_by_default, 1);
    ASSERT_EQ("init: modal_policy=HIDE_ON_ESC", p.modal_policy, COFI_MODAL_HIDE_ON_ESC);
    ASSERT_NULL("init: row_count=NULL", (void *)p.row_count);
    ASSERT_NULL("init: id=NULL", (void *)p.id);
    ASSERT_NULL("init: shortcut_hint=NULL", (void *)p.shortcut_hint);
    ASSERT_NULL("init: get_shortcut_hint=NULL", (void *)p.get_shortcut_hint);
    ASSERT_NULL("init: on_enter_pressed=NULL", (void *)p.on_enter_pressed);
    ASSERT_NULL("init: handle_key=NULL", (void *)p.handle_key);
    ASSERT_EQ("init: required=0", p.required, 0);
    ASSERT_EQ("init: tick_interval_ms=0", p.tick_interval_ms, 0);
    ASSERT_EQ("init: initial_selection_index=0", p.initial_selection_index, 0);
    ASSERT_EQ("init: slot_store_enabled=0", p.slot_store_enabled, 0);
}

static void test_registry_add_and_get(void) {
    cofi_registry_reset();

    CofiTabProvider p1, p2;
    cofi_init_provider_defaults(&p1);
    p1.id = "first";
    p1.tab_mode = 99;
    p1.row_count = mock_row_count_5;

    cofi_init_provider_defaults(&p2);
    p2.id = "second";
    p2.tab_mode = 100;
    p2.row_count = mock_row_count_3;

    int id1 = cofi_register_tab_provider(&p1);
    int id2 = cofi_register_tab_provider(&p2);

    ASSERT_EQ("id1=0", id1, 0);
    ASSERT_EQ("id2=1", id2, 1);
    ASSERT_EQ("provider_count=2", cofi_provider_count(), 2);

    const CofiTabProvider *r1 = cofi_get_provider(id1);
    const CofiTabProvider *r2 = cofi_get_provider(id2);
    ASSERT_NOT_NULL("get provider 0", r1);
    ASSERT_NOT_NULL("get provider 1", r2);
    ASSERT_STR_EQ("provider 0 id", r1->id, "first");
    ASSERT_STR_EQ("provider 1 id", r2->id, "second");
    ASSERT_EQ("get provider id by string", cofi_get_provider_id("second"), id2);
    ASSERT_EQ("unknown provider id string returns -1", cofi_get_provider_id("missing"), -1);
    ASSERT_NULL("get out-of-range returns NULL", cofi_get_provider(99));
    ASSERT_NULL("get negative id returns NULL", cofi_get_provider(-1));
}

static void test_dynamic_tab_assignment(void) {
    cofi_registry_reset();

    CofiTabProvider legacy, dynamic_one, dynamic_two;
    cofi_init_provider_defaults(&legacy);
    legacy.id = "legacy";
    legacy.tab_mode = TAB_APPS;
    int legacy_id = cofi_register_tab_provider(&legacy);

    cofi_init_provider_defaults(&dynamic_one);
    dynamic_one.id = "dynamic-one";
    dynamic_one.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    int dynamic_one_id = cofi_register_tab_provider(&dynamic_one);

    cofi_init_provider_defaults(&dynamic_two);
    dynamic_two.id = "dynamic-two";
    dynamic_two.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    int dynamic_two_id = cofi_register_tab_provider(&dynamic_two);

    const CofiTabProvider *first = cofi_get_provider(dynamic_one_id);
    const CofiTabProvider *second = cofi_get_provider(dynamic_two_id);
    ASSERT_EQ("legacy provider registered", legacy_id, 0);
    ASSERT_NOT_NULL("dynamic provider one exists", first);
    ASSERT_NOT_NULL("dynamic provider two exists", second);
    ASSERT_EQ("first dynamic tab starts after legacy enum",
              first ? first->tab_mode : -1, TAB_COUNT + 1);
    ASSERT_EQ("second dynamic tab increments",
              second ? second->tab_mode : -1, TAB_COUNT + 2);
    ASSERT_NOT_NULL("dynamic tab resolves by handle",
                    cofi_get_provider_for_tab(TAB_COUNT + 1));

    int tabs[8];
    int count = cofi_list_provider_tabs(tabs, 8);
    ASSERT_EQ("provider tab list count", count, 3);
    ASSERT_EQ("legacy tab ordered before dynamic tabs", tabs[0], TAB_APPS);
    ASSERT_EQ("dynamic tab one listed", tabs[1], TAB_COUNT + 1);
    ASSERT_EQ("dynamic tab two listed", tabs[2], TAB_COUNT + 2);
}

static void test_get_provider_for_tab(void) {
    cofi_registry_reset();

    CofiTabProvider p;
    cofi_init_provider_defaults(&p);
    p.id = "mytab";
    p.tab_mode = 42;
    cofi_register_tab_provider(&p);

    const CofiTabProvider *found = cofi_get_provider_for_tab(42);
    ASSERT_NOT_NULL("found by tab_mode", found);
    ASSERT_STR_EQ("correct provider found", found->id, "mytab");
    ASSERT_NULL("unknown tab_mode returns NULL", cofi_get_provider_for_tab(999));
}

static void test_disabled_provider_runtime_lookups_are_hidden(void) {
    static const char *aliases[] = { "alias", NULL };
    cofi_registry_reset();

    CofiTabProvider p;
    cofi_init_provider_defaults(&p);
    p.id = "disabled";
    p.tab_mode = 43;
    p.primary_cmd = "primary";
    p.aliases = aliases;
    p.prefix_char = '!';
    p.row_count = mock_row_count_5;
    p.on_command_args = mock_command_args;
    int id = cofi_register_tab_provider(&p);

    ASSERT_TRUE("provider starts enabled", cofi_provider_is_enabled(id));
    ASSERT_NOT_NULL("enabled lookup by tab", cofi_get_provider_for_tab(43));
    ASSERT_NOT_NULL("enabled lookup by primary command", cofi_get_provider_for_command("primary"));
    ASSERT_NOT_NULL("enabled lookup by alias", cofi_get_provider_for_command("alias"));
    ASSERT_NOT_NULL("enabled lookup by prefix", cofi_get_provider_for_prefix('!'));

    cofi_set_provider_enabled(id, 0);

    ASSERT_TRUE("provider is now disabled", !cofi_provider_is_enabled(id));
    ASSERT_NOT_NULL("raw lookup by id still works", cofi_get_provider(id));
    ASSERT_NULL("disabled lookup by tab hidden", cofi_get_provider_for_tab(43));
    ASSERT_NULL("disabled lookup by primary hidden", cofi_get_provider_for_command("primary"));
    ASSERT_NULL("disabled lookup by alias hidden", cofi_get_provider_for_command("alias"));
    ASSERT_NULL("disabled lookup by prefix hidden", cofi_get_provider_for_prefix('!'));
    ASSERT_EQ("disabled row_count dispatch suppressed", cofi_call_row_count(id, NULL), 0);
    ASSERT_EQ("disabled command dispatch suppressed",
              cofi_call_on_command_args(id, NULL, "x"), COFI_NO_OP);

    cofi_set_provider_enabled(id, 1);

    ASSERT_TRUE("provider re-enabled", cofi_provider_is_enabled(id));
    ASSERT_NOT_NULL("re-enabled lookup by tab", cofi_get_provider_for_tab(43));
    ASSERT_EQ("re-enabled row_count dispatch works", cofi_call_row_count(id, NULL), 5);
}

static void test_apply_disabled_provider_list(void) {
    cofi_registry_reset();

    CofiTabProvider config, profiles, sinks;
    cofi_init_provider_defaults(&config);
    config.id = "config";
    config.tab_mode = 10;
    config.required = 1;
    int config_id = cofi_register_tab_provider(&config);

    cofi_init_provider_defaults(&profiles);
    profiles.id = "profiles";
    profiles.tab_mode = 11;
    int profiles_id = cofi_register_tab_provider(&profiles);

    cofi_init_provider_defaults(&sinks);
    sinks.id = "sinks";
    sinks.tab_mode = 12;
    int sinks_id = cofi_register_tab_provider(&sinks);

    cofi_apply_disabled_providers(" profiles, unknown sinks config ");

    ASSERT_TRUE("config provider is not disableable", !cofi_provider_is_disableable(config_id));
    ASSERT_TRUE("profiles provider is disableable", cofi_provider_is_disableable(profiles_id));
    ASSERT_TRUE("config remains enabled", cofi_provider_is_enabled(config_id));
    ASSERT_TRUE("profiles disabled from list", !cofi_provider_is_enabled(profiles_id));
    ASSERT_TRUE("sinks disabled from list", !cofi_provider_is_enabled(sinks_id));

    char disabled[128] = {0};
    cofi_build_disabled_providers_string(disabled, sizeof(disabled));
    ASSERT_STR_EQ("disabled list is canonical registry order", disabled, "profiles,sinks");

    cofi_apply_disabled_providers("");
    ASSERT_TRUE("empty disabled list re-enables profiles", cofi_provider_is_enabled(profiles_id));
    ASSERT_TRUE("empty disabled list re-enables sinks", cofi_provider_is_enabled(sinks_id));
}

static void test_disableable_uses_required_metadata_not_id(void) {
    cofi_registry_reset();

    CofiTabProvider config_named, required_custom;
    cofi_init_provider_defaults(&config_named);
    config_named.id = "config";
    config_named.tab_mode = 20;
    int config_named_id = cofi_register_tab_provider(&config_named);

    cofi_init_provider_defaults(&required_custom);
    required_custom.id = "custom-required";
    required_custom.tab_mode = 21;
    required_custom.required = 1;
    int required_custom_id = cofi_register_tab_provider(&required_custom);

    ASSERT_TRUE("config id alone is disableable",
                cofi_provider_is_disableable(config_named_id));
    ASSERT_TRUE("required metadata blocks disable",
                !cofi_provider_is_disableable(required_custom_id));

    cofi_set_provider_enabled(config_named_id, 0);
    cofi_set_provider_enabled(required_custom_id, 0);

    ASSERT_TRUE("config-named non-required provider can be disabled",
                !cofi_provider_is_enabled(config_named_id));
    ASSERT_TRUE("required provider remains enabled",
                cofi_provider_is_enabled(required_custom_id));
}

static void test_filtered_raw_mapping(void) {
    cofi_registry_reset();

    CofiTabProvider p;
    cofi_init_provider_defaults(&p);
    p.id = "maptest";
    p.tab_mode = 1;
    int id = cofi_register_tab_provider(&p);

    ASSERT_EQ("initial filtered count=0", cofi_get_filtered_count(id), 0);
    ASSERT_EQ("initial filtered_to_raw returns -1", cofi_filtered_to_raw(id, 0), -1);

    int raw_map[] = {3, 7, 1, 9, 2};
    cofi_set_filtered_map(id, raw_map, 5);

    ASSERT_EQ("filtered count=5", cofi_get_filtered_count(id), 5);
    ASSERT_EQ("filtered[0]=3", cofi_filtered_to_raw(id, 0), 3);
    ASSERT_EQ("filtered[1]=7", cofi_filtered_to_raw(id, 1), 7);
    ASSERT_EQ("filtered[4]=2", cofi_filtered_to_raw(id, 4), 2);
    ASSERT_EQ("out-of-range returns -1", cofi_filtered_to_raw(id, 5), -1);
    ASSERT_EQ("negative index returns -1", cofi_filtered_to_raw(id, -1), -1);
    ASSERT_EQ("bad provider_id returns -1", cofi_filtered_to_raw(99, 0), -1);

    /* overwrite with smaller map */
    int map2[] = {10, 20};
    cofi_set_filtered_map(id, map2, 2);
    ASSERT_EQ("overwrite count=2", cofi_get_filtered_count(id), 2);
    ASSERT_EQ("overwrite[0]=10", cofi_filtered_to_raw(id, 0), 10);
    ASSERT_EQ("old index 4 now out-of-range", cofi_filtered_to_raw(id, 4), -1);
}

static void test_generation_token(void) {
    cofi_registry_reset();

    CofiTabProvider p;
    cofi_init_provider_defaults(&p);
    p.id = "gentest";
    p.tab_mode = 2;
    int id = cofi_register_tab_provider(&p);

    ASSERT_EQ("initial generation=0", cofi_current_generation(id), 0);

    int g1 = cofi_next_generation(id);
    ASSERT_EQ("next_generation=1", g1, 1);
    ASSERT_EQ("current_generation=1", cofi_current_generation(id), 1);

    int g2 = cofi_next_generation(id);
    ASSERT_EQ("next_generation=2", g2, 2);

    ASSERT_EQ("bad id next returns -1", cofi_next_generation(99), -1);
    ASSERT_EQ("bad id current returns -1", cofi_current_generation(99), -1);
}

static void test_dispatch_helpers(void) {
    cofi_registry_reset();

    CofiTabProvider p;
    cofi_init_provider_defaults(&p);
    p.id = "dispatch";
    p.tab_mode = 3;
    p.row_count = mock_row_count_5;
    p.on_enter_pressed = mock_enter_pressed;
    int id = cofi_register_tab_provider(&p);

    ASSERT_EQ("row_count dispatch=5", cofi_call_row_count(id, NULL), 5);

    CofiActionStatus st = cofi_call_on_enter_pressed(id, NULL, 0, 0, "expr", 0);
    ASSERT_EQ("on_enter_pressed dispatch=KEEP", st, COFI_HANDLED_KEEP);

    ASSERT_EQ("row_count bad id=0", cofi_call_row_count(99, NULL), 0);
    ASSERT_EQ("on_command_args bad id=NO_OP",
              cofi_call_on_command_args(99, NULL, "x"), COFI_NO_OP);

    /* provider with no on_enter_pressed returns NO_OP */
    CofiTabProvider p2;
    cofi_init_provider_defaults(&p2);
    p2.id = "noop";
    p2.tab_mode = 4;
    int id2 = cofi_register_tab_provider(&p2);
    ASSERT_EQ("no on_enter_pressed returns NO_OP",
              cofi_call_on_enter_pressed(id2, NULL, 0, 0, "", 0), COFI_NO_OP);
}

static void test_multiple_providers_independent(void) {
    cofi_registry_reset();

    CofiTabProvider a, b;
    cofi_init_provider_defaults(&a);
    a.id = "A";
    a.tab_mode = 10;

    cofi_init_provider_defaults(&b);
    b.id = "B";
    b.tab_mode = 11;

    int id_a = cofi_register_tab_provider(&a);
    int id_b = cofi_register_tab_provider(&b);

    int map_a[] = {0, 1, 2};
    int map_b[] = {5, 6};
    cofi_set_filtered_map(id_a, map_a, 3);
    cofi_set_filtered_map(id_b, map_b, 2);

    ASSERT_EQ("A filtered count=3", cofi_get_filtered_count(id_a), 3);
    ASSERT_EQ("B filtered count=2", cofi_get_filtered_count(id_b), 2);
    ASSERT_EQ("A[2]=2", cofi_filtered_to_raw(id_a, 2), 2);
    ASSERT_EQ("B[0]=5", cofi_filtered_to_raw(id_b, 0), 5);

    cofi_next_generation(id_a);
    ASSERT_EQ("A gen=1", cofi_current_generation(id_a), 1);
    ASSERT_EQ("B gen unchanged=0", cofi_current_generation(id_b), 0);
}

int main(void) {
    printf("CofiTabProvider registry tests\n");
    printf("==============================\n\n");

    test_init_defaults();
    test_registry_add_and_get();
    test_dynamic_tab_assignment();
    test_get_provider_for_tab();
    test_disabled_provider_runtime_lookups_are_hidden();
    test_apply_disabled_provider_list();
    test_disableable_uses_required_metadata_not_id();
    test_filtered_raw_mapping();
    test_generation_token();
    test_dispatch_helpers();
    test_multiple_providers_independent();

    printf("\n==============================\n");
    printf("Results: %d/%d tests passed\n", s_tests_passed, s_tests_run);
    return s_tests_passed == s_tests_run ? 0 : 1;
}
