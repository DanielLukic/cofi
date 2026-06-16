#include "profiles/chrome_launch.h"

#include <string.h>

static gchar *default_program_resolver(const char *program) {
    return g_find_program_in_path(program);
}

static ChromeLaunchProgramResolver s_program_resolver = default_program_resolver;

char **chrome_launch_build_argv(const char *chrome_path,
                                const char *profile_dir,
                                const char *url,
                                gboolean new_window) {
    int has_url = (url && url[0] != '\0') ? 1 : 0;
    int has_new_window = has_url && new_window ? 1 : 0;
    char **argv = g_new0(char *, (gsize)(3 + has_url + has_new_window));
    int i = 0;
    argv[i++] = g_strdup(chrome_path ? chrome_path : "");
    argv[i++] = g_strdup_printf("--profile-directory=%s", profile_dir ? profile_dir : "");
    if (has_new_window) {
        argv[i++] = g_strdup("--new-window");
    }
    if (has_url) {
        argv[i++] = g_strdup(url);
    }
    argv[i] = NULL;
    return argv;
}

gchar *chrome_launch_resolve_executable(const BrowserProfileEntry *entry) {
    if (!entry || entry->backend != BROWSER_PROFILE_CHROME) {
        return NULL;
    }
    gchar *path = s_program_resolver(entry->executable);
    if (!path && strcmp(entry->executable, "google-chrome") == 0) {
        path = s_program_resolver("google-chrome-stable");
    }
    return path;
}

#ifdef COFI_TESTING
void chrome_launch_set_program_resolver_test_hook(ChromeLaunchProgramResolver resolver) {
    s_program_resolver = resolver ? resolver : default_program_resolver;
}
#endif
