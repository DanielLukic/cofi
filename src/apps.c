#include "apps.h"
#include "log.h"
#include "system_actions.h"
#include "detach_launch.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct {
    AppEntry entry;
    score_t score;
} ScoredAppEntry;

static AppEntry s_entries[MAX_APPS];
static int s_count = 0;
static GList *s_app_list = NULL;

/* ---- Pure helpers (testable without GIO) ---- */

static int app_entry_cmp(const void *a, const void *b) {
    return g_utf8_collate(((const AppEntry *)a)->name,
                          ((const AppEntry *)b)->name);
}

static int scored_app_entry_cmp(const void *a, const void *b) {
    const ScoredAppEntry *left = (const ScoredAppEntry *)a;
    const ScoredAppEntry *right = (const ScoredAppEntry *)b;

    if (left->score > right->score) {
        return -1;
    }
    if (left->score < right->score) {
        return 1;
    }

    return g_utf8_collate(left->entry.name, right->entry.name);
}

static int is_token_separator(char ch) {
    return isspace((unsigned char)ch) ||
           ch == ';' || ch == ',' || ch == '/' ||
           ch == '-' || ch == '_' || ch == '.' ||
           ch == '(' || ch == ')' || ch == ':';
}

static int ascii_prefix_len_ci(const char *query, const char *text) {
    int matched = 0;

    while (query[matched] && text[matched]) {
        if (tolower((unsigned char)query[matched]) !=
            tolower((unsigned char)text[matched])) {
            break;
        }
        matched++;
    }

    return matched;
}

static score_t score_token_match(const char *query, const char *token, size_t token_len,
                                 score_t base_score) {
    char token_buf[512];
    int prefix_len;

    if (!query || !*query || !token || token_len == 0 || token_len >= sizeof(token_buf)) {
        return SCORE_MIN;
    }

    memcpy(token_buf, token, token_len);
    token_buf[token_len] = '\0';

    if (!has_match(query, token_buf)) {
        return SCORE_MIN;
    }

    prefix_len = ascii_prefix_len_ci(query, token_buf);
    if (query[prefix_len] == '\0') {
        return base_score + 2000;
    }

    return base_score + match(query, token_buf);
}

static score_t score_tokenized_field(const char *query, const char *field, score_t base_score) {
    score_t best = SCORE_MIN;

    if (!query || !*query || !field || !*field) {
        return SCORE_MIN;
    }

    for (const char *p = field; *p;) {
        while (*p && is_token_separator(*p)) {
            p++;
        }

        const char *start = p;
        while (*p && !is_token_separator(*p)) {
            p++;
        }

        size_t len = (size_t)(p - start);
        score_t token_score = score_token_match(query, start, len, base_score);
        if (token_score > best) {
            best = token_score;
        }
    }

    return best;
}

static score_t score_name_match(const char *query, const char *name) {
    score_t best = SCORE_MIN;

    if (!query || !*query || !name || !*name) {
        return SCORE_MIN;
    }

    if (has_match(query, name)) {
        best = 300000 + match(query, name);
    }

    score_t token_score = score_tokenized_field(query, name, 320000);
    if (token_score > best) {
        best = token_score;
    }

    return best;
}

static score_t score_app_entry(const char *query, const AppEntry *entry) {
    score_t best = SCORE_MIN;

    if (!query || !*query || !entry) {
        return SCORE_MIN;
    }

    score_t name_score = score_name_match(query, entry->name);
    if (name_score > best) {
        best = name_score;
    }

    score_t generic_score = score_tokenized_field(query, entry->generic_name, 200000);
    if (generic_score > best) {
        best = generic_score;
    }

    score_t keyword_score = score_tokenized_field(query, entry->keywords, 100000);
    if (keyword_score > best) {
        best = keyword_score;
    }

    return best;
}

void apps_sort_entries(AppEntry *entries, int count) {
    qsort(entries, (size_t)count, sizeof(AppEntry), app_entry_cmp);
}

void apps_filter_entries(const char *query,
                         AppEntry *src, int src_count,
                         AppEntry *out, int *out_count) {
    ScoredAppEntry scored[MAX_APPS];
    int scored_count = 0;

    *out_count = 0;
    for (int i = 0; i < src_count; i++) {
        if (!query || !*query) {
            out[(*out_count)++] = src[i];
            continue;
        }

        score_t score = score_app_entry(query, &src[i]);
        if (score > SCORE_MIN && scored_count < MAX_APPS) {
            scored[scored_count].entry = src[i];
            scored[scored_count].score = score;
            scored_count++;
        }
    }

    qsort(scored, (size_t)scored_count, sizeof(ScoredAppEntry), scored_app_entry_cmp);

    for (int i = 0; i < scored_count; i++) {
        out[(*out_count)++] = scored[i].entry;
    }
}

/* ---- GIO-dependent implementation ---- */

static void populate_entry(AppEntry *e, GAppInfo *info) {
    const char *n = g_app_info_get_name(info);
    strncpy(e->name, n ? n : "", sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';

    e->generic_name[0] = '\0';
    e->keywords[0] = '\0';

    if (G_IS_DESKTOP_APP_INFO(info)) {
        GDesktopAppInfo *di = G_DESKTOP_APP_INFO(info);

        const char *gn = g_desktop_app_info_get_generic_name(di);
        if (gn) {
            strncpy(e->generic_name, gn, sizeof(e->generic_name) - 1);
            e->generic_name[sizeof(e->generic_name) - 1] = '\0';
        }

        const char * const *kw = g_desktop_app_info_get_keywords(di);
        if (kw) {
            for (int i = 0; kw[i]; i++) {
                if (i > 0) {
                    strncat(e->keywords, " ",
                            sizeof(e->keywords) - strlen(e->keywords) - 1);
                }
                strncat(e->keywords, kw[i],
                        sizeof(e->keywords) - strlen(e->keywords) - 1);
            }
        }
    }

    e->source_kind = APP_SOURCE_DESKTOP;
    e->action_id = SYSTEM_ACTION_NONE;
    e->exec_path[0] = '\0';
    e->info = info;
}

void apps_unload(void) {
    if (s_app_list) {
        g_list_free_full(s_app_list, g_object_unref);
        s_app_list = NULL;
    }
    s_count = 0;
}

void apps_load(void) {
    const gint64 total_start_us = g_get_monotonic_time();
    apps_unload();

    const gint64 desktop_start_us = g_get_monotonic_time();
    s_app_list = g_app_info_get_all();

    int desktop_count = 0;
    for (GList *l = s_app_list; l && s_count < MAX_APPS; l = l->next) {
        GAppInfo *info = G_APP_INFO(l->data);

        if (!g_app_info_should_show(info)) continue;

        const char *name = g_app_info_get_name(info);
        if (!name || !*name) continue;

        populate_entry(&s_entries[s_count++], info);
        desktop_count++;
    }
    const double desktop_ms = (double)(g_get_monotonic_time() - desktop_start_us) / 1000.0;

    const gint64 system_start_us = g_get_monotonic_time();
    int system_count = 0;
    if (s_count < MAX_APPS) {
        system_actions_load(&s_entries[s_count], &system_count, MAX_APPS - s_count);
        s_count += system_count;
    }
    const double system_ms = (double)(g_get_monotonic_time() - system_start_us) / 1000.0;

    apps_sort_entries(s_entries, s_count);

    const double total_ms = (double)(g_get_monotonic_time() - total_start_us) / 1000.0;
    log_info("Apps: loaded %d entries in %.2fms (desktop=%d in %.2fms, system=%d in %.2fms)",
             s_count,
             total_ms,
             desktop_count,
             desktop_ms,
             system_count,
             system_ms);
}

void apps_filter(const char *query, AppEntry *out, int *out_count) {
    apps_filter_entries(query, s_entries, s_count, out, out_count);
}

void apps_launch(const AppEntry *entry) {
    if (!entry) {
        return;
    }

    if (entry->source_kind == APP_SOURCE_SYSTEM) {
        system_actions_invoke(entry);
        return;
    }

    if (entry->source_kind == APP_SOURCE_PATH) {
        if (!detach_launch_in_terminal(entry->exec_path)) {
            log_error("Failed to launch PATH binary '%s'", entry->exec_path);
        }
        return;
    }

    if (!entry->info) {
        return;
    }

    GError *err = NULL;
    if (!g_app_info_launch(entry->info, NULL, NULL, &err)) {
        log_error("Failed to launch '%s': %s",
                  entry->name, err ? err->message : "unknown error");
        if (err) g_error_free(err);
    } else {
        log_info("Launched app: %s", entry->name);
    }
}
