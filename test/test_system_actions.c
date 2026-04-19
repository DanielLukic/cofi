#include <stdio.h>
#include <string.h>

#include "../src/system_actions.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

#define ASSERT_EQ_INT(name, expected, actual) \
    ASSERT_TRUE(name, (expected) == (actual))

static int find_index_by_name(const AppEntry *entries, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void test_load_exact_actions(void) {
    AppEntry out[16];
    int count = 0;

    system_actions_load(out, &count, 16);

    ASSERT_EQ_INT("system actions count", 6, count);
    ASSERT_TRUE("has Lock", find_index_by_name(out, count, "Lock") >= 0);
    ASSERT_TRUE("has Suspend", find_index_by_name(out, count, "Suspend") >= 0);
    ASSERT_TRUE("has Hibernate", find_index_by_name(out, count, "Hibernate") >= 0);
    ASSERT_TRUE("has Logout", find_index_by_name(out, count, "Logout") >= 0);
    ASSERT_TRUE("has Reboot", find_index_by_name(out, count, "Reboot") >= 0);
    ASSERT_TRUE("has Shutdown", find_index_by_name(out, count, "Shutdown") >= 0);
}

static void test_metadata_fields(void) {
    AppEntry out[16];
    int count = 0;

    system_actions_load(out, &count, 16);

    for (int i = 0; i < count; i++) {
        ASSERT_EQ_INT("source_kind is system", APP_SOURCE_SYSTEM, out[i].source_kind);
        ASSERT_TRUE("name non-empty", out[i].name[0] != '\0');
        ASSERT_TRUE("name null-terminated", memchr(out[i].name, '\0', sizeof(out[i].name)) != NULL);
        ASSERT_TRUE("action_id set", out[i].action_id != SYSTEM_ACTION_NONE);
    }
}

static void test_keywords_synonyms(void) {
    AppEntry out[16];
    int count = 0;

    system_actions_load(out, &count, 16);

    const int shutdown_i = find_index_by_name(out, count, "Shutdown");
    const int lock_i = find_index_by_name(out, count, "Lock");
    const int suspend_i = find_index_by_name(out, count, "Suspend");

    ASSERT_TRUE("shutdown found", shutdown_i >= 0);
    ASSERT_TRUE("shutdown has poweroff synonym",
                shutdown_i >= 0 && strstr(out[shutdown_i].keywords, "poweroff") != NULL);
    ASSERT_TRUE("lock has session synonym",
                lock_i >= 0 && strstr(out[lock_i].keywords, "session") != NULL);
    ASSERT_TRUE("suspend has standby synonym",
                suspend_i >= 0 && strstr(out[suspend_i].keywords, "standby") != NULL);
}

static void test_max_cap_respected(void) {
    AppEntry out[3];
    int count = 999;

    system_actions_load(out, &count, 3);

    ASSERT_EQ_INT("count capped by max", 3, count);
    ASSERT_TRUE("first is Lock", strcmp(out[0].name, "Lock") == 0);
    ASSERT_TRUE("third is Hibernate", strcmp(out[2].name, "Hibernate") == 0);
}

static void test_deterministic_load(void) {
    AppEntry out_a[16];
    AppEntry out_b[16];
    int count_a = 0;
    int count_b = 0;

    system_actions_load(out_a, &count_a, 16);
    system_actions_load(out_b, &count_b, 16);

    ASSERT_EQ_INT("deterministic count", count_a, count_b);

    for (int i = 0; i < count_a; i++) {
        ASSERT_TRUE("deterministic names", strcmp(out_a[i].name, out_b[i].name) == 0);
        ASSERT_TRUE("deterministic keywords", strcmp(out_a[i].keywords, out_b[i].keywords) == 0);
        ASSERT_EQ_INT("deterministic source", out_a[i].source_kind, out_b[i].source_kind);
        ASSERT_EQ_INT("deterministic action id", out_a[i].action_id, out_b[i].action_id);
    }
}

static void test_invoke_smoke_noop_paths(void) {
    AppEntry desktop_entry;
    memset(&desktop_entry, 0, sizeof(desktop_entry));
    desktop_entry.source_kind = APP_SOURCE_DESKTOP;

    AppEntry unknown_action;
    memset(&unknown_action, 0, sizeof(unknown_action));
    unknown_action.source_kind = APP_SOURCE_SYSTEM;
    unknown_action.action_id = SYSTEM_ACTION_NONE;
    strcpy(unknown_action.name, "Unknown");

    system_actions_invoke(NULL);
    system_actions_invoke(&desktop_entry);
    system_actions_invoke(&unknown_action);

    ASSERT_TRUE("invoke noop smoke completed", 1);
}

int main(void) {
    printf("System actions tests\n");
    printf("====================\n\n");

    test_load_exact_actions();
    test_metadata_fields();
    test_keywords_synonyms();
    test_max_cap_respected();
    test_deterministic_load();
    test_invoke_smoke_noop_paths();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
