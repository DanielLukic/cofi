#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config/config.h"
#include "core/app/app_data.h"
#include "projects/locate/projects_locate.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(name, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", name); \
        } else { \
            printf("FAIL: %s (line %d)\n", name, __LINE__); \
        } \
    } while (0)

static char g_script_path[512];
static int g_callback_count;
static int g_callback_result_count;
static guint g_callback_generation;
static char g_callback_query[128];
static char g_callback_paths[64][1024];
static char g_last_spawn_query[256];

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

static GSubprocess *spawn_test_script(const char *tool_path,
                                      const char *glob_query,
                                      GError **error) {
    (void)tool_path;
    g_strlcpy(g_last_spawn_query, glob_query ? glob_query : "", sizeof(g_last_spawn_query));
    const gchar *argv[] = {g_script_path, glob_query, NULL};
    return g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE, error);
}

static void on_locate_results(AppData *app,
                              const char *query,
                              guint generation,
                              const ProjectsLocateResult *results,
                              int result_count) {
    (void)app;
    (void)results;
    g_callback_count++;
    g_callback_result_count = result_count;
    g_callback_generation = generation;
    g_strlcpy(g_callback_query, query ? query : "", sizeof(g_callback_query));
    for (int i = 0; i < result_count && i < 64; i++) {
        g_strlcpy(g_callback_paths[i],
                  results[i].path ? results[i].path : "",
                  sizeof(g_callback_paths[i]));
    }
}

static void init_test_config(CofiConfig *config) {
    memset(config, 0, sizeof(*config));
    config->projects_locate_enabled = 1;
    config->projects_locate_timeout_ms = 1500;
    g_strlcpy(config->projects_locate_excludes,
              "~/.cache/*,~/.local/*,~/.config/*,~/.var/app/*,~/snap/*,~/.gradle/*,~/.npm/*,~/.nvm/*,*/node_modules/*,*/__pycache__/*,*/.git/*,*/caches/*,*/cache/*",
              sizeof(config->projects_locate_excludes));
    config->projects_locate_search_roots[0] = '\0';
}

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    init_test_config(&app->config);
    init_projects_mode(&app->projects_mode);
    g_callback_count = 0;
    g_callback_result_count = 0;
    g_callback_generation = 0;
    g_callback_query[0] = '\0';
    g_last_spawn_query[0] = '\0';
    memset(g_callback_paths, 0, sizeof(g_callback_paths));
    projects_locate_reset_for_test();
    projects_locate_set_results_callback(on_locate_results);
    projects_locate_set_spawn_impl_for_test(spawn_test_script);
    projects_locate_set_tool_path_for_test("/test/plocate");
}

static gboolean loop_until(gboolean (*predicate)(gpointer), gpointer data, guint timeout_ms) {
    GMainContext *ctx = g_main_context_default();
    gint64 deadline = g_get_monotonic_time() + (gint64)timeout_ms * 1000;
    while (!predicate(data) && g_get_monotonic_time() < deadline) {
        while (g_main_context_pending(ctx)) {
            g_main_context_iteration(ctx, FALSE);
        }
        g_usleep(1000);
    }
    return predicate(data);
}

static gboolean callback_seen(gpointer data) {
    (void)data;
    return g_callback_count > 0;
}

static gboolean pending_cleared(gpointer data) {
    (void)data;
    return !projects_locate_has_pending_for_test();
}

static void write_script(const char *path, const char *body) {
    FILE *f = fopen(path, "w");
    ASSERT_TRUE("create test script", f != NULL);
    if (!f) return;
    fputs("#!/bin/sh\n", f);
    fputs(body, f);
    fclose(f);
    chmod(path, 0755);
}

static char *make_temp_dir(void) {
    char tmpl[] = "/tmp/cofi-projects-locate-XXXXXX";
    char *dir = g_strdup(tmpl);
    ASSERT_TRUE("mkdtemp dir", mkdtemp(dir) != NULL);
    return dir;
}

static void test_glob_builder_interleaves_stars_between_query_chars(void) {
    gchar *glob = projects_locate_glob_for_test("bh");
    ASSERT_TRUE("bh becomes b*h*",
                strcmp(glob, "b*h*") == 0);
    g_free(glob);

    glob = projects_locate_glob_for_test("harn");
    ASSERT_TRUE("harn becomes h*a*r*n*",
                strcmp(glob, "h*a*r*n*") == 0);
    g_free(glob);

    glob = projects_locate_glob_for_test("a");
    ASSERT_TRUE("single char query gets trailing star",
                strcmp(glob, "a*") == 0);
    g_free(glob);
}

static void test_exclude_matching_uses_default_globs(void) {
    CofiConfig config;
    init_test_config(&config);
    char cache_path[512];
    char chrome_path[512];
    g_snprintf(cache_path, sizeof(cache_path), "%s/.cache/tool", g_get_home_dir());
    g_snprintf(chrome_path, sizeof(chrome_path), "%s/.config/google-chrome/Default", g_get_home_dir());
    ASSERT_TRUE("exclude matches node_modules",
                projects_locate_path_excluded_for_test(&config,
                                                       "/tmp/proj/x/node_modules/y"));
    ASSERT_TRUE("exclude matches home cache",
                projects_locate_path_excluded_for_test(&config, cache_path));
    ASSERT_TRUE("exclude matches chrome config noise",
                projects_locate_path_excluded_for_test(&config, chrome_path));
    ASSERT_TRUE("exclude allows project path",
                !projects_locate_path_excluded_for_test(&config,
                                                        "/tmp/proj/sample"));
}

static void test_search_roots_filters_candidates_when_configured(void) {
    AppData app;
    reset_state(&app);

    char *root = make_temp_dir();
    char *outside = make_temp_dir();
    g_strlcpy(app.config.projects_locate_search_roots, root,
              sizeof(app.config.projects_locate_search_roots));

    char *in_root = g_build_filename(root, "sample", NULL);
    char *out_root = g_build_filename(outside, "sample", NULL);
    g_mkdir_with_parents(in_root, 0755);
    g_mkdir_with_parents(out_root, 0755);

    char *script_dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/roots.sh", script_dir);
    gchar *body = g_strdup_printf("printf '%%s\\n' %s\nprintf '%%s\\n' %s\n",
                                  in_root, out_root);
    write_script(g_script_path, body);
    g_free(body);

    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "har", 1);

    ASSERT_TRUE("search roots callback arrives", loop_until(callback_seen, NULL, 2000));
    ASSERT_TRUE("search roots drops outside prefix",
                g_callback_result_count == 1 &&
                strcmp(g_callback_paths[0], in_root) == 0);

    g_free(in_root);
    g_free(out_root);
    g_free(root);
    g_free(outside);
    g_free(script_dir);
}

static void test_empty_search_roots_leaves_search_unbounded(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/unbounded.sh", dir);
    write_script(g_script_path,
                 "printf '%s\\n' /tmp/bm-home\n"
                 "printf '%s\\n' /usr/share/bm-host\n");

    g_mkdir_with_parents("/tmp/bm-home", 0755);
    g_mkdir_with_parents("/usr/share/bm-host", 0755);
    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "bmh", 1);

    ASSERT_TRUE("empty roots callback arrives", loop_until(callback_seen, NULL, 2000));
    ASSERT_TRUE("empty roots does not prefilter by prefix",
                g_callback_result_count >= 1 &&
                strcmp(g_callback_paths[0], "/tmp/bm-home") == 0);
    g_free(dir);
}

static void test_generation_guard_discards_stale_callback(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/stale.sh", dir);
    write_script(g_script_path, "sleep 0.2\nprintf '%s\\n' /tmp/old-result\n");

    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "old", 1);

    g_snprintf(g_script_path, sizeof(g_script_path), "%s/fresh.sh", dir);
    write_script(g_script_path, "printf '%s\\n' /tmp/new-result\n");
    app.projects_mode.locate_generation = 2;
    projects_locate_search_async(&app, "new", 2);

    ASSERT_TRUE("fresh callback arrives", loop_until(callback_seen, NULL, 2000));
    ASSERT_TRUE("only fresh callback delivered", g_callback_count == 1);
    ASSERT_TRUE("fresh generation delivered", g_callback_generation == 2);
    ASSERT_TRUE("fresh query delivered", strcmp(g_callback_query, "new") == 0);
    g_free(dir);
}

static void test_cap_enforcement_limits_to_fifty_results(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    char dirs_root[512];
    g_snprintf(dirs_root, sizeof(dirs_root), "%s/dirs", dir);
    g_mkdir_with_parents(dirs_root, 0755);

    g_snprintf(g_script_path, sizeof(g_script_path), "%s/cap.sh", dir);
    FILE *script = fopen(g_script_path, "w");
    ASSERT_TRUE("open cap script", script != NULL);
    if (script) {
        fputs("#!/bin/sh\n", script);
        for (int i = 0; i < 100; i++) {
            char entry[512];
            g_snprintf(entry, sizeof(entry), "%s/d%02d", dirs_root, i);
            g_mkdir(entry, 0755);
            fprintf(script, "printf '%%s\\n' '%s'\n", entry);
        }
        fclose(script);
        chmod(g_script_path, 0755);
    }

    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "ddd", 1);

    ASSERT_TRUE("cap callback arrives", loop_until(callback_seen, NULL, 3000));
    ASSERT_TRUE("cap limits results to 50", g_callback_result_count == 50);
    g_free(dir);
}

static void test_fzf_basename_ranking_prefers_boundary_match(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    char best_path[512];
    char bluetooth_path[512];
    char bashtop_path[512];
    g_snprintf(best_path, sizeof(best_path), "%s/ba-host", dir);
    g_snprintf(bluetooth_path, sizeof(bluetooth_path), "%s/bluetooth", dir);
    g_snprintf(bashtop_path, sizeof(bashtop_path), "%s/bashtop", dir);
    g_mkdir(best_path, 0755);
    g_mkdir(bluetooth_path, 0755);
    g_mkdir(bashtop_path, 0755);
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/rank.sh", dir);
    FILE *script = fopen(g_script_path, "w");
    ASSERT_TRUE("open rank script", script != NULL);
    if (script) {
        fputs("#!/bin/sh\n", script);
        fprintf(script, "printf '%%s\\n' '%s'\n", best_path);
        fprintf(script, "printf '%%s\\n' '%s'\n", bluetooth_path);
        fprintf(script, "printf '%%s\\n' '%s'\n", bashtop_path);
        fclose(script);
        chmod(g_script_path, 0755);
    }

    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "bh", 1);

    ASSERT_TRUE("ranking callback arrives", loop_until(callback_seen, NULL, 2000));
    ASSERT_TRUE("query is converted to glob for plocate",
                strcmp(g_last_spawn_query, "b*h*") == 0);
    ASSERT_TRUE("boundary-bonus name ranks first for bh",
                g_callback_result_count >= 1 &&
                strcmp(g_callback_paths[0], best_path) == 0);
    g_free(dir);
}

static void test_length_tiebreaker_prefers_shorter_path_when_scores_match(void) {
    AppData app;
    reset_state(&app);

    char *short_base = make_temp_dir();
    char *deep_base = make_temp_dir();
    char *short_path = g_build_filename(short_base, "proj", NULL);
    char *deep_path = g_build_filename(deep_base, "a", "b", "c", "d", "proj", NULL);
    g_mkdir_with_parents(short_path, 0755);
    g_mkdir_with_parents(deep_path, 0755);

    char *script_dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/length.sh", script_dir);
    gchar *body = g_strdup_printf("printf '%%s\\n' %s\nprintf '%%s\\n' %s\n",
                                  short_path, deep_path);
    write_script(g_script_path, body);
    g_free(body);

    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "prj", 1);

    ASSERT_TRUE("length callback arrives", loop_until(callback_seen, NULL, 2000));
    ASSERT_TRUE("shorter path wins same-name tie",
                g_callback_result_count >= 2 &&
                strcmp(g_callback_paths[0], short_path) == 0);

    g_free(short_path);
    g_free(deep_path);
    g_free(short_base);
    g_free(deep_base);
    g_free(script_dir);
}

static void test_empty_plocate_result_is_delivered_quietly(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/empty.sh", dir);
    write_script(g_script_path, "exit 1\n");

    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "bharn", 1);

    ASSERT_TRUE("empty locate callback arrives", loop_until(callback_seen, NULL, 2000));
    ASSERT_TRUE("empty locate result count is zero", g_callback_result_count == 0);
    g_free(dir);
}

static void test_expanded_excludes_remove_noise_paths(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    char home_config_path[512];
    char cache_path[512];
    char caches_path[512];
    char canonical_path[512];
    g_snprintf(home_config_path, sizeof(home_config_path),
               "%s/.config/google-chrome/Profile 1", g_get_home_dir());
    g_snprintf(cache_path, sizeof(cache_path),
               "%s/projects/example/cache/sample", g_get_home_dir());
    g_snprintf(caches_path, sizeof(caches_path),
               "%s/projects/example/caches/sample", g_get_home_dir());
    g_snprintf(canonical_path, sizeof(canonical_path),
               "%s/projects/sample", g_get_home_dir());
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/excludes.sh", dir);
    FILE *script = fopen(g_script_path, "w");
    ASSERT_TRUE("open excludes script", script != NULL);
    if (script) {
        fputs("#!/bin/sh\n", script);
        fprintf(script, "printf '%%s\\n' '%s'\n", home_config_path);
        fprintf(script, "printf '%%s\\n' '%s'\n", cache_path);
        fprintf(script, "printf '%%s\\n' '%s'\n", caches_path);
        fprintf(script, "printf '%%s\\n' '%s'\n", canonical_path);
        fclose(script);
        chmod(g_script_path, 0755);
    }

    g_mkdir_with_parents(home_config_path, 0755);
    g_mkdir_with_parents(cache_path, 0755);
    g_mkdir_with_parents(caches_path, 0755);
    g_mkdir_with_parents(canonical_path, 0755);
    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "har", 1);

    ASSERT_TRUE("exclude callback arrives", loop_until(callback_seen, NULL, 2000));
    ASSERT_TRUE("only canonical project survives excludes",
                g_callback_result_count == 1 &&
                strcmp(g_callback_paths[0], canonical_path) == 0);
    g_free(dir);
}

static void test_timeout_cancels_without_mutating_results(void) {
    AppData app;
    reset_state(&app);
    app.config.projects_locate_timeout_ms = 100;

    char *dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/timeout.sh", dir);
    write_script(g_script_path, "sleep 5\n");

    app.projects_mode.locate_generation = 1;
    projects_locate_search_async(&app, "slow", 1);

    ASSERT_TRUE("timeout eventually clears pending op",
                loop_until(pending_cleared, NULL, 2000));
    ASSERT_TRUE("timeout does not deliver callback", g_callback_count == 0);
    g_free(dir);
}

int main(void) {
    printf("projects_locate behavioral tests\n");
    printf("================================\n\n");

    test_glob_builder_interleaves_stars_between_query_chars();
    test_exclude_matching_uses_default_globs();
    test_search_roots_filters_candidates_when_configured();
    test_empty_search_roots_leaves_search_unbounded();
    test_generation_guard_discards_stale_callback();
    test_cap_enforcement_limits_to_fifty_results();
    test_fzf_basename_ranking_prefers_boundary_match();
    test_length_tiebreaker_prefers_shorter_path_when_scores_match();
    test_empty_plocate_result_is_delivered_quietly();
    test_expanded_excludes_remove_noise_paths();
    test_timeout_cancels_without_mutating_results();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
