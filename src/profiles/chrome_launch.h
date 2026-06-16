#ifndef CHROME_LAUNCH_H
#define CHROME_LAUNCH_H

#include <glib.h>

#include "profiles/browser_profiles.h"

/* Pure-helpers for launching Chrome with a specific profile and optional URL.
 *
 * Split out of browser_profiles.c so the bookmarks subsystem can reuse argv
 * construction and executable resolution without depending on the full
 * BrowserProfilesMode + filtering surface.
 *
 * Behavior contract (must remain stable for callers):
 * - chrome_launch_build_argv: returns a NULL-terminated argv array containing
 *   the resolved chrome path, --profile-directory=<profile_dir>, and the URL
 *   when non-NULL and non-empty. When new_window is TRUE and URL is present,
 *   --new-window is inserted before the URL. Caller owns the returned strv
 *   (g_strfreev).
 * - chrome_launch_resolve_executable: looks up the entry's executable on PATH,
 *   falling back to google-chrome-stable for Chrome backends when the primary
 *   name is missing. Returns NULL when no candidate is on PATH.
 */
char **chrome_launch_build_argv(const char *chrome_path,
                                const char *profile_dir,
                                const char *url,
                                gboolean new_window);

gchar *chrome_launch_resolve_executable(const BrowserProfileEntry *entry);

typedef gchar *(*ChromeLaunchProgramResolver)(const char *program);

#ifdef COFI_TESTING
void chrome_launch_set_program_resolver_test_hook(ChromeLaunchProgramResolver resolver);
#endif

#endif /* CHROME_LAUNCH_H */
