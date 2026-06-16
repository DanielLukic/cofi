#include "matching/tier_score.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "core/log/log.h"
#include "core/utils/constants.h"
#include "matching/fzf_algo.h"

static bool is_word_start(const char *s, int pos) {
    if (pos == 0) return true;
    char p = s[pos - 1];
    return p == ' ' || p == '-' || p == '_' || p == '.' ||
           p == '(' || p == '|' || p == '/';
}

// Signal B: count adjacent-word-start pairs for the best alignment of the
// query against the display's word-start positions.
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
    int eff_ws = ws_count < WS_CAP ? ws_count : WS_CAP;

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
                if (dp[i - 1][k] < 0) continue;
                int candidate = dp[i - 1][k] + (k + 1 == j ? 1 : 0);
                if (candidate > dp[i][j]) dp[i][j] = candidate;
            }
        }
    }

    int best = 0;
    for (int j = 0; j < eff_ws; j++) {
        if (dp[eff_flen - 1][j] > best) best = dp[eff_flen - 1][j];
    }
    return best;
}

// Returns true when the query appears as a contiguous case-insensitive
// sequence of characters starting at a word-boundary position in display.
static bool is_direct_word_boundary_match(const char *filter, const char *display) {
    int flen = strlen(filter);
    if (flen == 0) return false;
    for (int i = 0; display[i]; i++) {
        if (i > 0 && !is_word_start(display, i)) continue;
        int j = 0;
        while (j < flen && display[i + j] &&
               tolower((unsigned char)display[i + j]) == tolower((unsigned char)filter[j]))
            j++;
        if (j == flen) return true;
    }
    return false;
}

score_t tier_score_string(const char *filter, const char *display) {
    if (!fzf_has_match(filter, display)) {
        return SCORE_MIN;
    }

    score_t score = fzf_fuzzy_match(filter, display);

    int pairs = consecutive_word_start_pairs(filter, display);
    if (pairs > 0) {
        score += pairs * CONSECUTIVE_WORD_START_PAIR_BONUS;
        log_debug("WORD_START_PAIRS: '%s' -> '%s' pairs=%d (+%.0f)",
                  filter, display, pairs,
                  (score_t)pairs * CONSECUTIVE_WORD_START_PAIR_BONUS);
    }

    if (is_direct_word_boundary_match(filter, display)) {
        score += TIER_DIRECT_BASE;
        log_debug("TIER_DIRECT: '%s' -> '%s' (%.0f)", filter, display, score);
    } else {
        if (score > (score_t)INDIRECT_SCORE_MAX)
            score = (score_t)INDIRECT_SCORE_MAX;
        log_debug("TIER_INDIRECT: '%s' -> '%s' (%.0f)", filter, display, score);
    }

    return score;
}
