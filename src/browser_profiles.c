#include "browser_profiles.h"

#include "detach_launch.h"
#include "fzf_algo.h"
#include "log.h"

#include <json-glib/json-glib.h>
#include <stdio.h>

static gchar *default_program_resolver(const char *program) {
    return g_find_program_in_path(program);
}

static BrowserProfilesProgramResolver s_program_resolver = default_program_resolver;
static BrowserProfilesLaunchImpl s_launch_impl = detach_launch_argv_array;

static void safe_copy(char *dest, size_t size, const char *src) {
    if (!dest || size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    g_strlcpy(dest, src, size);
}

static const char *json_string_member(JsonObject *object, const char *member) {
    if (!object || !json_object_has_member(object, member)) {
        return "";
    }
    JsonNode *node = json_object_get_member(object, member);
    return node && JSON_NODE_HOLDS_VALUE(node) ? json_node_get_string(node) : "";
}

static double json_double_member(JsonObject *object, const char *member) {
    if (!object || !json_object_has_member(object, member)) {
        return 0.0;
    }
    JsonNode *node = json_object_get_member(object, member);
    return node && JSON_NODE_HOLDS_VALUE(node) ? json_node_get_double(node) : 0.0;
}

static int profile_active_time_cmp(const void *lhs, const void *rhs) {
    const BrowserProfileEntry *left = (const BrowserProfileEntry *)lhs;
    const BrowserProfileEntry *right = (const BrowserProfileEntry *)rhs;
    if (left->active_time < right->active_time) return 1;
    if (left->active_time > right->active_time) return -1;
    return g_ascii_strcasecmp(left->name, right->name);
}

int browser_profiles_parse_chrome_local_state(const char *contents,
                                              BrowserProfileEntry *out,
                                              int max_out,
                                              char *error_out,
                                              size_t error_size) {
    if (error_out && error_size > 0) {
        error_out[0] = '\0';
    }
    if (!out || max_out <= 0) {
        return 0;
    }
    if (!contents || contents[0] == '\0') {
        if (error_out && error_size > 0)
            g_snprintf(error_out, error_size, "Chrome Local State is empty");
        return 0;
    }

    GError *error = NULL;
    JsonParser *parser = json_parser_new();
    gboolean ok = json_parser_load_from_data(parser, contents, -1, &error);
    if (!ok) {
        if (error_out && error_size > 0)
            g_snprintf(error_out, error_size, "Chrome Local State parse failed: %s",
                       error ? error->message : "unknown error");
        g_clear_error(&error);
        g_object_unref(parser);
        return 0;
    }

    JsonNode *root = json_parser_get_root(parser);
    JsonObject *root_obj = root && JSON_NODE_HOLDS_OBJECT(root) ? json_node_get_object(root) : NULL;
    JsonObject *profile_obj = root_obj && json_object_has_member(root_obj, "profile")
        ? json_object_get_object_member(root_obj, "profile") : NULL;
    JsonObject *info_cache = profile_obj && json_object_has_member(profile_obj, "info_cache")
        ? json_object_get_object_member(profile_obj, "info_cache") : NULL;

    int count = 0;
    if (info_cache) {
        GList *members = json_object_get_members(info_cache);
        for (GList *it = members; it && count < max_out; it = it->next) {
            const char *profile_dir = (const char *)it->data;
            JsonObject *profile = json_object_get_object_member(info_cache, profile_dir);
            const char *name = json_string_member(profile, "name");
            BrowserProfileEntry *entry = &out[count++];
            memset(entry, 0, sizeof(*entry));
            entry->backend = BROWSER_PROFILE_CHROME;
            safe_copy(entry->browser_id, sizeof(entry->browser_id), "chrome");
            safe_copy(entry->browser_name, sizeof(entry->browser_name), "Chrome");
            safe_copy(entry->executable, sizeof(entry->executable), "google-chrome");
            safe_copy(entry->profile_dir, sizeof(entry->profile_dir), profile_dir);
            safe_copy(entry->name, sizeof(entry->name), name[0] ? name : profile_dir);
            safe_copy(entry->email, sizeof(entry->email), json_string_member(profile, "user_name"));
            entry->active_time = json_double_member(profile, "active_time");
        }
        g_list_free(members);
    }

    g_object_unref(parser);
    qsort(out, (size_t)count, sizeof(out[0]), profile_active_time_cmp);

    if (count == 0 && error_out && error_size > 0) {
        g_snprintf(error_out, error_size, "No Chrome profiles found");
    }
    return count;
}

static int load_chrome_profiles(BrowserProfileEntry *out,
                                int max_out,
                                char *error_out,
                                size_t error_size) {
    gchar *path = g_build_filename(g_get_home_dir(), ".config", "google-chrome",
                                   "Local State", NULL);
    gchar *contents = NULL;
    gsize len = 0;
    GError *error = NULL;
    gboolean ok = g_file_get_contents(path, &contents, &len, &error);
    (void)len;
    if (!ok) {
        if (error_out && error_size > 0)
            g_snprintf(error_out, error_size, "Cannot read Chrome Local State: %s",
                       error ? error->message : path);
        g_clear_error(&error);
        g_free(path);
        return 0;
    }

    int count = browser_profiles_parse_chrome_local_state(contents, out, max_out,
                                                          error_out, error_size);
    g_free(contents);
    g_free(path);
    return count;
}

void browser_profiles_load(BrowserProfilesMode *mode) {
    if (!mode) return;
    mode->profile_count = load_chrome_profiles(mode->profiles, MAX_BROWSER_PROFILES,
                                               mode->last_error, sizeof(mode->last_error));
    if (mode->profile_count > 0) {
        mode->last_error[0] = '\0';
    }
    browser_profiles_filter(mode, "");
}

void browser_profiles_format_match_text(const BrowserProfileEntry *profile,
                                        char *out,
                                        size_t out_size) {
    if (!out || out_size == 0) return;
    if (!profile) {
        out[0] = '\0';
        return;
    }
    g_snprintf(out, out_size, "[gc] chrome google-chrome %s %s %s",
               profile->name, profile->email, profile->profile_dir);
}

typedef struct {
    int raw_index;
    score_t score;
} ProfileHit;

static score_t profile_field_score(const char *query,
                                   const char *field,
                                   score_t boost) {
    if (!field || field[0] == '\0' || !fzf_has_match(query, field)) {
        return SCORE_MIN;
    }
    return fzf_fuzzy_match(query, field) + boost;
}

static const char *email_domain(const char *email) {
    const char *at = email ? strchr(email, '@') : NULL;
    return at && at[1] ? at + 1 : "";
}

static score_t profile_composite_score(const char *query,
                                       const char *left,
                                       const char *right,
                                       score_t boost) {
    char text[512];
    g_snprintf(text, sizeof(text), "%s %s", left ? left : "", right ? right : "");
    return profile_field_score(query, text, boost);
}

static score_t profile_match_score(const BrowserProfileEntry *profile,
                                   const char *query) {
    if (!profile || !query || query[0] == '\0') {
        return SCORE_MIN;
    }

    score_t best = SCORE_MIN;
    score_t score = profile_field_score(query, profile->name, 3000);
    if (score > best) best = score;

    score = profile_field_score(query, profile->email, 2000);
    if (score > best) best = score;

    score = profile_field_score(query, profile->profile_dir, 1000);
    if (score > best) best = score;

    score = profile_composite_score(query, profile->name, profile->email, 1500);
    if (score > best) best = score;

    score = profile_composite_score(query, email_domain(profile->email), profile->name, 1500);
    if (score > best) best = score;

    score = profile_field_score(query, "gc", 100);
    if (score > best) best = score;

    score = profile_field_score(query, profile->browser_name, 50);
    if (score > best) best = score;

    score = profile_field_score(query, profile->browser_id, 50);
    if (score > best) best = score;

    return best;
}

static int profile_hit_cmp(const void *lhs, const void *rhs) {
    const ProfileHit *left = (const ProfileHit *)lhs;
    const ProfileHit *right = (const ProfileHit *)rhs;
    if (left->score > right->score) return -1;
    if (left->score < right->score) return 1;
    return left->raw_index - right->raw_index;
}

void browser_profiles_filter(BrowserProfilesMode *mode, const char *filter) {
    if (!mode) return;
    mode->filtered_count = 0;
    const char *query = filter ? filter : "";

    if (query[0] == '\0') {
        for (int i = 0; i < mode->profile_count; i++) {
            mode->filtered_indices[mode->filtered_count++] = i;
        }
        return;
    }

    ProfileHit hits[MAX_BROWSER_PROFILES];
    int hit_count = 0;
    for (int i = 0; i < mode->profile_count; i++) {
        score_t score = profile_match_score(&mode->profiles[i], query);
        if (score == SCORE_MIN) {
            continue;
        }
        hits[hit_count].raw_index = i;
        hits[hit_count].score = score;
        hit_count++;
    }

    qsort(hits, (size_t)hit_count, sizeof(hits[0]), profile_hit_cmp);
    for (int i = 0; i < hit_count; i++) {
        mode->filtered_indices[mode->filtered_count++] = hits[i].raw_index;
    }
}

static char **build_chrome_argv(const char *chrome_path, const char *profile_dir) {
    char **argv = g_new0(char *, 3);
    argv[0] = g_strdup(chrome_path);
    argv[1] = g_strdup_printf("--profile-directory=%s", profile_dir);
    argv[2] = NULL;
    return argv;
}

static gchar *resolve_browser_executable(const BrowserProfileEntry *profile) {
    if (!profile || profile->backend != BROWSER_PROFILE_CHROME) {
        return NULL;
    }
    gchar *path = s_program_resolver(profile->executable);
    if (!path && strcmp(profile->executable, "google-chrome") == 0) {
        path = s_program_resolver("google-chrome-stable");
    }
    return path;
}

gboolean browser_profiles_launch(const BrowserProfileEntry *profile) {
    if (!profile) return FALSE;
    gchar *browser_path = resolve_browser_executable(profile);
    if (!browser_path) {
        log_error("No browser executable found for %s profile '%s'",
                  profile->browser_name, profile->name);
        return FALSE;
    }

    char **argv = build_chrome_argv(browser_path, profile->profile_dir);
    gboolean ok = s_launch_impl((const char *const *)argv);
    if (ok) {
        log_info("Launched %s profile '%s' (%s) via %s",
                 profile->browser_name, profile->name, profile->profile_dir, browser_path);
    }
    g_strfreev(argv);
    g_free(browser_path);
    return ok;
}

#ifdef COFI_TESTING
void browser_profiles_set_program_resolver_test_hook(BrowserProfilesProgramResolver resolver) {
    s_program_resolver = resolver ? resolver : default_program_resolver;
}

void browser_profiles_set_launch_impl_test_hook(BrowserProfilesLaunchImpl launch_impl) {
    s_launch_impl = launch_impl ? launch_impl : detach_launch_argv_array;
}

char **browser_profiles_build_chrome_argv_for_test(const char *chrome_path,
                                                   const char *profile_dir) {
    return build_chrome_argv(chrome_path, profile_dir);
}
#endif
