#include "emoji_provider.h"

#include "app_data.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "emoji_data.h"
#include "fzf_algo.h"
#include "tab_switching.h"
#include "window_lifecycle.h"

#include <gtk/gtk.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    // Non-overlapping tier bases. Max intra-tier fzf+weight contribution ≈ 200×8=1600,
    // so no lower tier can ever cross into a higher one regardless of fzf score.
    // MRU is a stable-sort tiebreaker among equal scores, never an additive bonus.
    EMOJI_TIER6_ALIAS_EXACT  = 300000,  // aliases token exactly matches query
    EMOJI_TIER5_NAME_EXACT   = 200000,  // name_norm exactly equals single-word query
    EMOJI_TIER4_NAME_WORD    = 100000,  // a word in name_norm exactly matches
    EMOJI_TIER3_NAME_PREFIX  =  30000,  // name_norm starts with query token
    EMOJI_TIER2_WORD_PREFIX  =  15000,  // a name word starts with query token
    EMOJI_TIER1_ACRONYM      =   5000,  // consecutive word-start aligned chars
    // Tier 0 (pure fzf): base 0

    EMOJI_NAME_WEIGHT        = 8,
    EMOJI_ALIASES_WEIGHT     = 7,
    EMOJI_KEYWORDS_WEIGHT    = 2,
    EMOJI_MAX_TOKEN_LEN      = 63,
    EMOJI_GLYPH_MAX_BYTES    = 32,
};

static CofiTabProvider s_emoji_provider;
static int s_emoji_provider_id = -1;
static char (*s_history_glyphs)[EMOJI_GLYPH_MAX_BYTES] = NULL;
static int *s_history_indices = NULL;
static int s_history_count = 0;
static int s_history_capacity = 0;
static int s_history_loaded = 0;

static int emoji_history_ensure_capacity(int needed) {
    if (needed <= s_history_capacity) {
        return 1;
    }
    int new_capacity = s_history_capacity > 0 ? s_history_capacity : 64;
    while (new_capacity < needed) {
        if (new_capacity > INT_MAX / 2) {
            new_capacity = needed;
            break;
        }
        new_capacity *= 2;
    }

    int *new_indices = realloc(s_history_indices, sizeof(int) * (size_t)new_capacity);
    if (!new_indices) {
        return 0;
    }
    s_history_indices = new_indices;
    char (*new_glyphs)[EMOJI_GLYPH_MAX_BYTES] =
        realloc(s_history_glyphs, (size_t)new_capacity * EMOJI_GLYPH_MAX_BYTES);
    if (!new_glyphs) {
        return 0;
    }

    s_history_glyphs = new_glyphs;
    for (int i = s_history_capacity; i < new_capacity; i++) {
        s_history_indices[i] = -1;
    }
    s_history_capacity = new_capacity;
    return 1;
}

static const char *emoji_history_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home || home[0] == '\0') {
        home = ".";
    }
    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/emoji_history.json", home);
    return path;
}

static int emoji_history_find_table_index(const char *glyph) {
    if (!glyph || glyph[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < EMOJI_TABLE_LEN; i++) {
        if (strcmp(EMOJI_TABLE[i].glyph, glyph) == 0) {
            return i;
        }
    }
    return -1;
}

static void emoji_history_reindex(void) {
    for (int i = 0; i < s_history_count; i++) {
        s_history_indices[i] = emoji_history_find_table_index(s_history_glyphs[i]);
    }
}

static int emoji_history_position(int table_idx) {
    if (table_idx < 0) {
        return -1;
    }
    for (int i = 0; i < s_history_count; i++) {
        if (s_history_indices[i] == table_idx) {
            return i;
        }
    }
    return -1;
}

static void emoji_history_load(void) {
    if (s_history_loaded) {
        return;
    }
    s_history_loaded = 1;
    s_history_count = 0;

    FILE *f = fopen(emoji_history_path(), "r");
    if (!f) {
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *glyph_start = strstr(line, "\"glyph\": \"");
        if (!glyph_start) {
            continue;
        }
        glyph_start += 10;
        char *glyph_end = strchr(glyph_start, '"');
        if (!glyph_end) {
            continue;
        }
        if (!emoji_history_ensure_capacity(s_history_count + 1)) {
            continue;
        }
        size_t glyph_len = (size_t)(glyph_end - glyph_start);
        if (glyph_len >= EMOJI_GLYPH_MAX_BYTES) {
            glyph_len = EMOJI_GLYPH_MAX_BYTES - 1;
        }
        memcpy(s_history_glyphs[s_history_count], glyph_start, glyph_len);
        s_history_glyphs[s_history_count][glyph_len] = '\0';
        s_history_count++;
    }

    fclose(f);
    emoji_history_reindex();
}

static void emoji_history_save(void) {
    FILE *f = fopen(emoji_history_path(), "w");
    if (!f) {
        return;
    }
    fprintf(f, "{\n  \"picks\": [\n");
    for (int i = 0; i < s_history_count; i++) {
        fprintf(f, "    {\"glyph\": \"%s\"}%s\n",
                s_history_glyphs[i], (i < s_history_count - 1) ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

static void emoji_history_push(const char *glyph) {
    if (!glyph || glyph[0] == '\0') {
        return;
    }

    int existing = -1;
    for (int i = 0; i < s_history_count; i++) {
        if (strcmp(s_history_glyphs[i], glyph) == 0) {
            existing = i;
            break;
        }
    }

    if (existing == 0) {
        s_history_indices[0] = emoji_history_find_table_index(glyph);
        return;
    }

    if (existing > 0) {
        for (int i = existing; i > 0; i--) {
            memcpy(s_history_glyphs[i], s_history_glyphs[i - 1], EMOJI_GLYPH_MAX_BYTES);
            s_history_indices[i] = s_history_indices[i - 1];
        }
    } else {
        if (!emoji_history_ensure_capacity(s_history_count + 1)) {
            return;
        }
        s_history_count++;
        for (int i = s_history_count - 1; i > 0; i--) {
            memcpy(s_history_glyphs[i], s_history_glyphs[i - 1], EMOJI_GLYPH_MAX_BYTES);
            s_history_indices[i] = s_history_indices[i - 1];
        }
    }

    strncpy(s_history_glyphs[0], glyph, EMOJI_GLYPH_MAX_BYTES - 1);
    s_history_glyphs[0][EMOJI_GLYPH_MAX_BYTES - 1] = '\0';
    s_history_indices[0] = emoji_history_find_table_index(s_history_glyphs[0]);
}

static TabMode emoji_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_emoji_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static int emoji_row_count(AppData *app) {
    return app ? app->filtered_emoji_count : 0;
}

static int string_has_token(const char *tokens, const char *query) {
    if (!tokens || !query || query[0] == '\0') {
        return 0;
    }

    size_t qlen = strlen(query);
    const char *p = tokens;
    while (*p) {
        while (*p == ' ') p++;
        if (*p == '\0') break;
        const char *start = p;
        while (*p && *p != ' ') p++;
        size_t len = (size_t)(p - start);
        if (len == qlen && strncmp(start, query, qlen) == 0) {
            return 1;
        }
    }
    return 0;
}

static int name_has_word(const char *name, const char *query) {
    if (!name || !query || query[0] == '\0') {
        return 0;
    }

    size_t qlen = strlen(query);
    const char *p = name;
    while (*p) {
        while (*p && (*p < 'a' || *p > 'z') && (*p < '0' || *p > '9')) p++;
        if (*p == '\0') break;
        const char *start = p;
        while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) p++;
        size_t len = (size_t)(p - start);
        if (len == qlen && strncmp(start, query, qlen) == 0) {
            return 1;
        }
    }
    return 0;
}

static int name_has_prefix(const char *name, const char *token) {
    if (!name || !token || token[0] == '\0') return 0;
    size_t tlen = strlen(token);
    return strncmp(name, token, tlen) == 0;
}

static int name_has_word_prefix(const char *name, const char *token) {
    if (!name || !token || token[0] == '\0') return 0;
    size_t tlen = strlen(token);
    const char *p = name;
    while (*p) {
        while (*p && ((*p < 'a' || *p > 'z') && (*p < '0' || *p > '9'))) p++;
        if (*p == '\0') break;
        if (strncmp(p, token, tlen) == 0) {
            return 1;
        }
        while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) p++;
    }
    return 0;
}

static int is_word_start(const char *s, int pos) {
    if (pos == 0) return 1;
    char p = s[pos - 1];
    return p == ' ' || p == '-' || p == '_' || p == '.' ||
           p == '(' || p == '|' || p == '/';
}

static int consecutive_word_start_pairs(const char *filter, const char *display) {
    int flen = (int)strlen(filter);
    if (flen < 2) return 0;

    int ws[256];
    int ws_count = 0;
    for (int i = 0; display[i] && ws_count < 256; i++) {
        if (is_word_start(display, i))
            ws[ws_count++] = i;
    }
    if (ws_count == 0) return 0;

    enum { FLEN_CAP = 64, WS_CAP = 128 };
    int eff_flen = flen < FLEN_CAP ? flen : FLEN_CAP;
    int eff_ws   = ws_count < WS_CAP ? ws_count : WS_CAP;

    int dp[FLEN_CAP][WS_CAP];
    for (int i = 0; i < eff_flen; i++)
        for (int j = 0; j < eff_ws; j++)
            dp[i][j] = -1;

    for (int j = 0; j < eff_ws; j++) {
        if (tolower((unsigned char)display[ws[j]]) == tolower((unsigned char)filter[0]))
            dp[0][j] = 0;
    }

    for (int i = 1; i < eff_flen; i++) {
        for (int j = 0; j < eff_ws; j++) {
            if (tolower((unsigned char)display[ws[j]]) != tolower((unsigned char)filter[i]))
                continue;
            for (int k = 0; k < j; k++) {
                if (dp[i-1][k] < 0) continue;
                int candidate = dp[i-1][k] + (k + 1 == j ? 1 : 0);
                if (candidate > dp[i][j]) dp[i][j] = candidate;
            }
        }
    }

    int best = 0;
    for (int j = 0; j < eff_ws; j++)
        if (dp[eff_flen-1][j] > best) best = dp[eff_flen-1][j];
    return best;
}

static int weighted_positive_score(score_t score, int weight) {
    if (score <= 0) {
        return 0;
    }
    double weighted = (double)score * (double)weight;
    if (weighted > (double)INT_MAX) {
        return INT_MAX;
    }
    return (int)weighted;
}

static char *normalize_query_ascii(const char *query) {
    if (!query) {
        return g_strdup("");
    }

    char *normalized = g_utf8_normalize(query, -1, G_NORMALIZE_NFKD);
    if (!normalized) {
        return g_strdup("");
    }

    GString *out = g_string_sized_new(strlen(query));
    const char *p = normalized;
    while (*p) {
        gunichar ch = g_utf8_get_char(p);
        p = g_utf8_next_char(p);
        if (g_unichar_combining_class(ch) != 0) {
            continue;
        }
        if (ch <= 0x7f) {
            char c = (char)ch;
            if (c >= 'A' && c <= 'Z') {
                c = (char)g_ascii_tolower(c);
            }
            g_string_append_c(out, c);
        }
    }

    g_free(normalized);
    return g_string_free(out, FALSE);
}

static int emoji_rank_score(const char *query, const EmojiEntry *entry) {
    int total = 0;
    int token_count = 0;
    int qlen;
    char token[EMOJI_MAX_TOKEN_LEN + 1];
    char *nq = normalize_query_ascii(query);
    const char *p = nq;

    while (*p) {
        while (*p && ((*p < 'a' || *p > 'z') && (*p < '0' || *p > '9'))) p++;
        if (*p == '\0') break;

        qlen = 0;
        while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) {
            if (qlen < EMOJI_MAX_TOKEN_LEN) token[qlen++] = *p;
            p++;
        }
        token[qlen] = '\0';
        token_count++;

        score_t name_score  = fzf_fuzzy_match(token, entry->name_norm);
        score_t alias_score = fzf_fuzzy_match(token, entry->aliases);
        score_t kw_score    = fzf_fuzzy_match(token, entry->keywords);

        int fzf_contrib = 0;
        if (name_score > 0)
            fzf_contrib = weighted_positive_score(name_score, EMOJI_NAME_WEIGHT);
        if (alias_score > 0) {
            int a = weighted_positive_score(alias_score, EMOJI_ALIASES_WEIGHT);
            if (a > fzf_contrib) fzf_contrib = a;
        }
        if (kw_score > 0) {
            int k = weighted_positive_score(kw_score, EMOJI_KEYWORDS_WEIGHT);
            if (k > fzf_contrib) fzf_contrib = k;
        }

        if (fzf_contrib <= 0) {
            g_free(nq);
            return 0;
        }

        int tier_base;
        if (string_has_token(entry->aliases, token)) {
            tier_base = EMOJI_TIER6_ALIAS_EXACT;
        } else if (strcmp(entry->name_norm, token) == 0) {
            tier_base = EMOJI_TIER5_NAME_EXACT;
        } else if (name_has_word(entry->name_norm, token)) {
            tier_base = EMOJI_TIER4_NAME_WORD;
        } else if (name_has_prefix(entry->name_norm, token)) {
            tier_base = EMOJI_TIER3_NAME_PREFIX;
        } else if (name_has_word_prefix(entry->name_norm, token)) {
            tier_base = EMOJI_TIER2_WORD_PREFIX;
        } else if (consecutive_word_start_pairs(token, entry->name_norm) > 0) {
            tier_base = EMOJI_TIER1_ACRONYM;
        } else {
            tier_base = 0;
        }

        total += tier_base + fzf_contrib;
    }

    if (token_count == 0) {
        g_free(nq);
        return 0;
    }

    g_free(nq);
    return total;
}

static void emoji_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_emoji_count) {
        out->cell_count = 0;
        out->row_flags = 0;
        return;
    }
    int actual_idx = app->filtered_emoji[raw_idx];
    const EmojiEntry *entry = &EMOJI_TABLE[actual_idx];

    out->cell_count = 2;
    out->cells[0].text = entry->glyph;
    out->cells[0].width_hint = 2;
    out->cells[0].align = 0;
    out->cells[1].text = entry->name;
    out->cells[1].width_hint = 0;
    out->cells[1].align = 0;
    out->row_flags = COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE;
}

static const char *emoji_match_string(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_emoji_count) {
        return NULL;
    }
    return EMOJI_TABLE[app->filtered_emoji[raw_idx]].keywords;
}

static const char *emoji_row_identity(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_emoji_count) {
        return NULL;
    }
    return EMOJI_TABLE[app->filtered_emoji[raw_idx]].glyph;
}

static CofiActionStatus emoji_on_enter_pressed(AppData *app, int filtered_idx,
                                               int raw_idx, const char *entry_text,
                                               int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_emoji_count) {
        return COFI_NO_OP;
    }
    int actual_idx = app->filtered_emoji[raw_idx];

    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, EMOJI_TABLE[actual_idx].glyph, -1);
    emoji_history_push(EMOJI_TABLE[actual_idx].glyph);
    emoji_history_save();
    return COFI_HANDLED_HIDE;
}

static void emoji_on_query_changed(AppData *app, const char *query) {
    int matched_idx[EMOJI_COUNT];
    int matched_score[EMOJI_COUNT];
    int matched_mru[EMOJI_COUNT];

    if (!app) return;
    app->filtered_emoji_count = 0;

    if (!query || query[0] == '\0') {
        for (int i = 0; i < EMOJI_TABLE_LEN; i++)
            app->filtered_emoji[app->filtered_emoji_count++] = i;
        return;
    }

    for (int i = 0; i < EMOJI_TABLE_LEN; i++) {
        int score = emoji_rank_score(query, &EMOJI_TABLE[i]);
        if (score > 0) {
            matched_idx[app->filtered_emoji_count]   = i;
            matched_score[app->filtered_emoji_count] = score;
            matched_mru[app->filtered_emoji_count]   = emoji_history_position(i);
            app->filtered_emoji_count++;
        }
    }

    // Sort: score desc; ties broken by MRU position asc (lower = more recent;
    // -1 = not in history → treated as INT_MAX = worst); then table index asc.
    for (int i = 0; i < app->filtered_emoji_count - 1; i++) {
        for (int j = i + 1; j < app->filtered_emoji_count; j++) {
            int si = matched_score[i], sj = matched_score[j];
            int mi = matched_mru[i] < 0 ? INT_MAX : matched_mru[i];
            int mj = matched_mru[j] < 0 ? INT_MAX : matched_mru[j];
            int do_swap = 0;
            if (sj > si) {
                do_swap = 1;
            } else if (sj == si) {
                if (mj < mi) do_swap = 1;
                else if (mj == mi && matched_idx[j] < matched_idx[i]) do_swap = 1;
            }
            if (do_swap) {
                int tmp;
                tmp = matched_score[i]; matched_score[i] = matched_score[j]; matched_score[j] = tmp;
                tmp = matched_idx[i];   matched_idx[i]   = matched_idx[j];   matched_idx[j]   = tmp;
                tmp = matched_mru[i];   matched_mru[i]   = matched_mru[j];   matched_mru[j]   = tmp;
            }
        }
    }

    for (int i = 0; i < app->filtered_emoji_count; i++)
        app->filtered_emoji[i] = matched_idx[i];

    // Scored results reorder on every keystroke.  Clearing the saved identity
    // makes restore_selection fall back to initial_index=0 (best match) instead
    // of chasing the prior selection to its new — potentially far — rank.
    app->selection.selected_provider_id[0] = '\0';
}

static void emoji_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    emoji_history_load();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "search emoji");
    emoji_on_query_changed(app, "");
}

static gboolean emoji_command_handler(AppData *app,
                                      WindowInfo *window __attribute__((unused)),
                                      const char *args __attribute__((unused))) {
    exit_command_mode(app);
    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, emoji_tab_mode());
    return FALSE;
}

static const CommandSpec s_emoji_command = {
    .primary = "emoji",
    .aliases = {NULL},
    .owner_provider_id = "emoji",
    .handler = emoji_command_handler,
    .description = "Switch to emoji tab",
    .help_format = "emoji",
    .keeps_open_on_hotkey_auto = 1
};

void emoji_provider_register(void) {
    cofi_init_provider_defaults(&s_emoji_provider);
    s_emoji_provider_id = -1;
    s_emoji_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_emoji_provider.id = "emoji";
    s_emoji_provider.display_name = "Emoji";
    s_emoji_provider.prefix_char = 0;
    s_emoji_provider.tab_prefix_chars = NULL;
    s_emoji_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_emoji_provider.initial_selection_index = 0;
    s_emoji_provider.row_count = emoji_row_count;
    s_emoji_provider.format_row = emoji_format_row;
    s_emoji_provider.match_string = emoji_match_string;
    s_emoji_provider.row_identity = emoji_row_identity;
    s_emoji_provider.on_enter_pressed = emoji_on_enter_pressed;
    s_emoji_provider.on_enter = emoji_on_enter;
    s_emoji_provider.on_query_changed = emoji_on_query_changed;
    s_emoji_provider_id = cofi_register_tab_provider(&s_emoji_provider);
    if (s_emoji_provider_id >= 0) {
        cofi_register_command(&s_emoji_command);
    }
}
