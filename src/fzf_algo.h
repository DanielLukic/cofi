/*
 * fzf_algo.h - Port of fzf's FuzzyMatchV2 algorithm to C
 *
 * Ported from: https://github.com/junegunn/fzf/blob/master/src/algo/algo.go
 *
 * This implements the Smith-Waterman variant used by fzf for fuzzy matching.
 * Unlike fzy's algorithm, fzf's approach does not unfairly penalize long
 * strings because gap penalties are bounded (scores floor at 0).
 */

#ifndef FZF_ALGO_H
#define FZF_ALGO_H

#include "match.h"  /* score_t, SCORE_MIN */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Perform fzf-style fuzzy matching of needle against haystack.
 *
 * Returns a score >= 0 if there is a match, or SCORE_MIN if no match.
 * Case-insensitive by default.
 */
score_t fzf_fuzzy_match(const char *needle, const char *haystack);

/*
 * Quick check whether needle is a subsequence of haystack.
 * Returns 1 if match exists, 0 otherwise. Case-insensitive.
 */
int fzf_has_match(const char *needle, const char *haystack);

#ifdef __cplusplus
}
#endif

#endif /* FZF_ALGO_H */
