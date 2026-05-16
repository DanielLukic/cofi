#include <stdio.h>
#include <string.h>

#include "../src/browser_profiles.h"

gboolean detach_launch_argv_array(const char *const *argv) {
    (void)argv;
    return TRUE;
}

static int tests_run = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(name, cond) do { \
    tests_run++; \
    if (cond) printf("PASS: %s\n", name); \
    else { printf("FAIL: %s\n", name); tests_failed++; } \
} while (0)

static const char *sample_local_state =
    "{"
    "  \"profile\": {"
    "    \"info_cache\": {"
    "      \"Profile 14\": {"
    "        \"name\": \"GS\","
    "        \"user_name\": \"profile.primary@example.test\","
    "        \"active_time\": 200.0"
    "      },"
    "      \"Default\": {"
    "        \"name\": \"Drew\","
    "        \"user_name\": \"profile.secondary@example.test\","
    "        \"active_time\": 100.0"
    "      }"
    "    }"
    "  }"
    "}";

int main(void) {
    BrowserProfileEntry profiles[MAX_BROWSER_PROFILES];
    char error[256];
    int count = browser_profiles_parse_chrome_local_state(sample_local_state,
                                                          profiles,
                                                          MAX_BROWSER_PROFILES,
                                                          error,
                                                          sizeof(error));
    ASSERT_TRUE("parse returns two profiles", count == 2);
    ASSERT_TRUE("newer profile sorted first", strcmp(profiles[0].profile_dir, "Profile 14") == 0);
    ASSERT_TRUE("profile display name parsed", strcmp(profiles[0].name, "GS") == 0);
    ASSERT_TRUE("profile email parsed", strcmp(profiles[0].email, "profile.primary@example.test") == 0);
    ASSERT_TRUE("profile browser id is chrome", strcmp(profiles[0].browser_id, "chrome") == 0);
    ASSERT_TRUE("profile executable is google-chrome", strcmp(profiles[0].executable, "google-chrome") == 0);

    char match_text[512];
    browser_profiles_format_match_text(&profiles[0], match_text, sizeof(match_text));
    ASSERT_TRUE("match text includes short marker", strstr(match_text, "[c]") != NULL);
    ASSERT_TRUE("match text includes chrome backend", strstr(match_text, "chrome") != NULL);
    ASSERT_TRUE("match text includes profile dir", strstr(match_text, "Profile 14") != NULL);

    char **argv = browser_profiles_build_chrome_argv_for_test("/usr/bin/google-chrome",
                                                              "Profile 14");
    ASSERT_TRUE("argv uses resolved chrome path",
                strcmp(argv[0], "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("argv includes profile-directory flag",
                strcmp(argv[1], "--profile-directory=Profile 14") == 0);
    ASSERT_TRUE("argv is NULL-terminated", argv[2] == NULL);
    g_strfreev(argv);

    count = browser_profiles_parse_chrome_local_state("{}", profiles,
                                                      MAX_BROWSER_PROFILES,
                                                      error,
                                                      sizeof(error));
    ASSERT_TRUE("empty json returns zero profiles", count == 0);
    ASSERT_TRUE("empty json reports error", error[0] != '\0');

    printf("\nResults: %d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
