#ifndef BROWSER_PROFILES_H
#define BROWSER_PROFILES_H

#include <glib.h>
#include <string.h>

#define MAX_BROWSER_PROFILES 64
#define MAX_BROWSER_ID_LEN 32
#define MAX_BROWSER_NAME_LEN 32
#define MAX_BROWSER_EXECUTABLE_LEN 64
#define MAX_BROWSER_PROFILE_DIR_LEN 128
#define MAX_BROWSER_PROFILE_LABEL_LEN 128
#define MAX_BROWSER_PROFILE_EMAIL_LEN 256
#define MAX_BROWSER_PROFILE_ERROR_LEN 256

typedef enum {
    BROWSER_PROFILE_CHROME,
} BrowserProfileBackend;

typedef struct {
    BrowserProfileBackend backend;
    char browser_id[MAX_BROWSER_ID_LEN];
    char browser_name[MAX_BROWSER_NAME_LEN];
    char executable[MAX_BROWSER_EXECUTABLE_LEN];
    char profile_dir[MAX_BROWSER_PROFILE_DIR_LEN];
    char name[MAX_BROWSER_PROFILE_LABEL_LEN];
    char email[MAX_BROWSER_PROFILE_EMAIL_LEN];
    double active_time;
} BrowserProfileEntry;

typedef struct {
    BrowserProfileEntry profiles[MAX_BROWSER_PROFILES];
    int filtered_indices[MAX_BROWSER_PROFILES];
    int profile_count;
    int filtered_count;
    char last_error[MAX_BROWSER_PROFILE_ERROR_LEN];
} BrowserProfilesMode;

typedef gchar *(*BrowserProfilesProgramResolver)(const char *program);
typedef gboolean (*BrowserProfilesLaunchImpl)(const char *const *argv);

static inline void init_browser_profiles_mode(BrowserProfilesMode *mode) {
    if (mode) {
        memset(mode, 0, sizeof(*mode));
    }
}

int browser_profiles_parse_chrome_local_state(const char *contents,
                                              BrowserProfileEntry *out,
                                              int max_out,
                                              char *error_out,
                                              size_t error_size);
void browser_profiles_load(BrowserProfilesMode *mode);
void browser_profiles_filter(BrowserProfilesMode *mode, const char *filter);
gboolean browser_profiles_launch(const BrowserProfileEntry *profile);
void browser_profiles_format_match_text(const BrowserProfileEntry *profile,
                                        char *out,
                                        size_t out_size);

#ifdef COFI_TESTING
void browser_profiles_set_program_resolver_test_hook(BrowserProfilesProgramResolver resolver);
void browser_profiles_set_launch_impl_test_hook(BrowserProfilesLaunchImpl launch_impl);
char **browser_profiles_build_chrome_argv_for_test(const char *chrome_path,
                                                   const char *profile_dir);
#endif

#endif /* BROWSER_PROFILES_H */
