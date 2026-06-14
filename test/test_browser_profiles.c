#include <stdio.h>
#include <string.h>

#include "profiles/browser_profiles.h"

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

static void seed_profile(BrowserProfileEntry *entry,
                         const char *name,
                         const char *email,
                         const char *profile_dir,
                         double active_time) {
    memset(entry, 0, sizeof(*entry));
    entry->backend = BROWSER_PROFILE_CHROME;
    g_strlcpy(entry->browser_id, "chrome", sizeof(entry->browser_id));
    g_strlcpy(entry->browser_name, "Chrome", sizeof(entry->browser_name));
    g_strlcpy(entry->executable, "google-chrome", sizeof(entry->executable));
    g_strlcpy(entry->name, name, sizeof(entry->name));
    g_strlcpy(entry->email, email, sizeof(entry->email));
    g_strlcpy(entry->profile_dir, profile_dir, sizeof(entry->profile_dir));
    entry->active_time = active_time;
}

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
    ASSERT_TRUE("match text includes short marker", strstr(match_text, "[gc]") != NULL);
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

    BrowserProfilesMode mode;
    init_browser_profiles_mode(&mode);
    mode.profile_count = 3;
    seed_profile(&mode.profiles[0], "Drew", "profile.secondary@example.test", "Default", 300.0);
    seed_profile(&mode.profiles[1], "Hannah", "profile.third@example.test", "Profile 11", 200.0);
    seed_profile(&mode.profiles[2], "GS", "profile.primary@example.test", "Profile 14", 100.0);

    browser_profiles_filter(&mode, "Hel");
    ASSERT_TRUE("profile-name fuzzy match wins over email-only match",
                mode.filtered_count > 0 && mode.filtered_indices[0] == 1);

    browser_profiles_filter(&mode, "stha");
    gboolean found_hannah = FALSE;
    for (int i = 0; i < mode.filtered_count; i++) {
        if (mode.filtered_indices[i] == 1) {
            found_hannah = TRUE;
            break;
        }
    }
    ASSERT_TRUE("domain-plus-name fuzzy match finds Hannah",
                mode.filtered_count > 0 && found_hannah);

    browser_profiles_filter(&mode, "gc");
    ASSERT_TRUE("browser marker can still show chrome profiles", mode.filtered_count == 3);

    /* Characterize the promoted browser_profiles_load_entries seam: when
     * HOME points at an empty dir, it returns 0 entries and a non-empty
     * error string. This pins the public Chrome-discovery entry point
     * behavior used by both profiles and bookmarks. */
    gchar *tmpdir = g_dir_make_tmp("cofi_bp_test_XXXXXX", NULL);
    g_setenv("HOME", tmpdir, TRUE);
    BrowserProfileEntry load_entries[MAX_BROWSER_PROFILES];
    char load_err[256] = {0};
    int load_count = browser_profiles_load_entries(load_entries,
                                                   MAX_BROWSER_PROFILES,
                                                   load_err,
                                                   sizeof(load_err));
    ASSERT_TRUE("load_entries returns 0 when Local State missing", load_count == 0);
    ASSERT_TRUE("load_entries populates error when Local State missing",
                load_err[0] != '\0');
    g_rmdir(tmpdir);
    g_free(tmpdir);

    printf("\nResults: %d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
