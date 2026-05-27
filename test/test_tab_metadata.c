#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "providers/cofi_tab_provider.h"
#include "ui/tab_metadata.h"

#define TEST_PROJECTS_TAB ((TabMode)(TAB_COUNT + 1))

const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    static CofiTabProvider projects_provider;
    if (tab_mode == TEST_PROJECTS_TAB) {
        memset(&projects_provider, 0, sizeof(projects_provider));
        projects_provider.id = "projects";
        projects_provider.display_name = "Projects";
        projects_provider.tab_mode = TEST_PROJECTS_TAB;
        return &projects_provider;
    }
    return NULL;
}

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s\n", msg); \
        } \
    } while (0)

static int non_empty(const char *text) {
    return text && text[0] != '\0';
}

static int all_upper_ascii(const char *text) {
    if (!non_empty(text)) return 0;
    for (const char *p = text; *p; p++) {
        if (isalpha((unsigned char)*p) && !isupper((unsigned char)*p)) {
            return 0;
        }
    }
    return 1;
}

static int all_lower_ascii(const char *text) {
    if (!non_empty(text)) return 0;
    for (const char *p = text; *p; p++) {
        if (isalpha((unsigned char)*p) && !islower((unsigned char)*p)) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    printf("Tab metadata tests\n");
    printf("==================\n\n");

    for (int tab = TAB_WINDOWS; tab < TAB_COUNT; tab++) {
        ASSERT_TRUE("display name exists", non_empty(tab_display_name((TabMode)tab)));
        ASSERT_TRUE("active name exists", non_empty(tab_active_name((TabMode)tab)));
        ASSERT_TRUE("log name exists", non_empty(tab_log_name((TabMode)tab)));
        ASSERT_TRUE("active name uppercase", all_upper_ascii(tab_active_name((TabMode)tab)));
        ASSERT_TRUE("log name lowercase", all_lower_ascii(tab_log_name((TabMode)tab)));
    }

    ASSERT_TRUE("projects tab display name", strcmp(tab_display_name(TEST_PROJECTS_TAB), "Projects") == 0);
    ASSERT_TRUE("projects tab active name", strcmp(tab_active_name(TEST_PROJECTS_TAB), "PROJECTS") == 0);
    ASSERT_TRUE("projects tab log name", strcmp(tab_log_name(TEST_PROJECTS_TAB), "projects") == 0);

    ASSERT_TRUE("TAB_COUNT has no display name", tab_display_name(TAB_COUNT) == NULL);
    ASSERT_TRUE("TAB_COUNT has no active name", tab_active_name(TAB_COUNT) == NULL);
    ASSERT_TRUE("TAB_COUNT has no log name", tab_log_name(TAB_COUNT) == NULL);

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
