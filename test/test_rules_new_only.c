#include <stdio.h>
#include <string.h>

#include "rules/rules.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

static Rule make_rule(bool new_only) {
    Rule rule;
    memset(&rule, 0, sizeof(rule));
    rule.new_only = new_only;
    return rule;
}

static void test_normal_rule_allows_all_triggers(void) {
    Rule rule = make_rule(false);

    ASSERT_TRUE("normal allows startup",
                rule_trigger_allows(&rule, RULE_TRIGGER_STARTUP, false) == true);
    ASSERT_TRUE("normal allows client-list new window",
                rule_trigger_allows(&rule, RULE_TRIGGER_CLIENT_LIST, true) == true);
    ASSERT_TRUE("normal allows client-list existing window",
                rule_trigger_allows(&rule, RULE_TRIGGER_CLIENT_LIST, false) == true);
    ASSERT_TRUE("normal allows title change",
                rule_trigger_allows(&rule, RULE_TRIGGER_TITLE_CHANGE, false) == true);
}

static void test_new_only_rule_allows_only_new_windows(void) {
    Rule rule = make_rule(true);

    ASSERT_TRUE("new-only blocks startup",
                rule_trigger_allows(&rule, RULE_TRIGGER_STARTUP, false) == false);
    ASSERT_TRUE("new-only allows client-list new window",
                rule_trigger_allows(&rule, RULE_TRIGGER_CLIENT_LIST, true) == true);
    ASSERT_TRUE("new-only blocks client-list existing window",
                rule_trigger_allows(&rule, RULE_TRIGGER_CLIENT_LIST, false) == false);
    ASSERT_TRUE("new-only blocks title change",
                rule_trigger_allows(&rule, RULE_TRIGGER_TITLE_CHANGE, false) == false);
}

int main(void) {
    printf("Rules new-only trigger tests\n");
    printf("============================\n\n");

    test_normal_rule_allows_all_triggers();
    test_new_only_rule_allows_only_new_windows();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
