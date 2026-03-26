/*
 * fzf_algo.c - Port of fzf's FuzzyMatchV2 algorithm to C
 *
 * Ported from: https://github.com/junegunn/fzf/blob/master/src/algo/algo.go
 *
 * Key differences from fzy's algorithm:
 * - Scores are floored at 0, so long gaps don't produce deeply negative scores
 * - Uses int16 scoring (we use int16_t internally, expose as score_t/double)
 * - Has a richer bonus system: boundary, camelCase, delimiter, whitespace
 * - First character match gets a multiplied bonus
 * - Consecutive match bonus is context-aware (tracks chunk start bonus)
 */

#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "fzf_algo.h"

/* --- Scoring constants (from fzf algo.go) --- */

#define SCORE_MATCH         16
#define SCORE_GAP_START     -3
#define SCORE_GAP_EXTENSION -1

/* Word boundary bonus: match score / 2 = 8 */
#define BONUS_BOUNDARY      (SCORE_MATCH / 2)

/* Non-word character bonus (same as boundary for computing consecutive chunks) */
#define BONUS_NON_WORD      (SCORE_MATCH / 2)

/* camelCase bonus: boundary + gap_extension = 8 + (-1) = 7 */
#define BONUS_CAMEL123      (BONUS_BOUNDARY + SCORE_GAP_EXTENSION)

/* Consecutive match bonus: -(gap_start + gap_extension) = -(-3 + -1) = 4 */
#define BONUS_CONSECUTIVE   (-(SCORE_GAP_START + SCORE_GAP_EXTENSION))

/* Extra bonus for word boundary after whitespace */
#define BONUS_BOUNDARY_WHITE     (BONUS_BOUNDARY + 2)

/* Extra bonus for word boundary after delimiter */
#define BONUS_BOUNDARY_DELIMITER (BONUS_BOUNDARY + 1)

/* First character bonus multiplier */
#define BONUS_FIRST_CHAR_MULTIPLIER 2

/* Maximum haystack/needle length we handle */
#define FZF_MAX_LEN 1024

/* --- Character classification (from fzf algo.go) --- */

typedef enum {
    CHAR_WHITE = 0,
    CHAR_NON_WORD,
    CHAR_DELIMITER,
    CHAR_LOWER,
    CHAR_UPPER,
    CHAR_LETTER,
    CHAR_NUMBER,
    CHAR_CLASS_COUNT
} char_class_t;

static const char *delimiter_chars = "/,:;|";
static const char *white_chars = " \t\n\v\f\r";

/* Initial char class: beginning of string is treated as whitespace boundary */
#define INITIAL_CHAR_CLASS CHAR_WHITE

static char_class_t ascii_char_classes[128];
static int16_t bonus_matrix[CHAR_CLASS_COUNT][CHAR_CLASS_COUNT];
static int fzf_initialized = 0;

/* Compute bonus for a transition from prev_class to class */
static int16_t bonus_for(char_class_t prev_class, char_class_t class) {
    if (class > CHAR_NON_WORD) {
        /* Current char is a "word" character (letter, number, etc.) */
        switch (prev_class) {
        case CHAR_WHITE:
            return BONUS_BOUNDARY_WHITE;
        case CHAR_DELIMITER:
            return BONUS_BOUNDARY_DELIMITER;
        case CHAR_NON_WORD:
            return BONUS_BOUNDARY;
        default:
            break;
        }
    }

    if (prev_class == CHAR_LOWER && class == CHAR_UPPER) {
        return BONUS_CAMEL123;
    }
    if (prev_class != CHAR_NUMBER && class == CHAR_NUMBER) {
        return BONUS_CAMEL123;
    }

    switch (class) {
    case CHAR_NON_WORD:
    case CHAR_DELIMITER:
        return BONUS_NON_WORD;
    case CHAR_WHITE:
        return BONUS_BOUNDARY_WHITE;
    default:
        break;
    }
    return 0;
}

static void fzf_init(void) {
    if (fzf_initialized)
        return;

    for (int i = 0; i < 128; i++) {
        char c = (char)i;
        if (c >= 'a' && c <= 'z') {
            ascii_char_classes[i] = CHAR_LOWER;
        } else if (c >= 'A' && c <= 'Z') {
            ascii_char_classes[i] = CHAR_UPPER;
        } else if (c >= '0' && c <= '9') {
            ascii_char_classes[i] = CHAR_NUMBER;
        } else if (strchr(white_chars, c) && c != '\0') {
            ascii_char_classes[i] = CHAR_WHITE;
        } else if (strchr(delimiter_chars, c) && c != '\0') {
            ascii_char_classes[i] = CHAR_DELIMITER;
        } else {
            ascii_char_classes[i] = CHAR_NON_WORD;
        }
    }

    for (int i = 0; i < CHAR_CLASS_COUNT; i++) {
        for (int j = 0; j < CHAR_CLASS_COUNT; j++) {
            bonus_matrix[i][j] = bonus_for((char_class_t)i, (char_class_t)j);
        }
    }

    fzf_initialized = 1;
}

static inline char_class_t char_class_of(unsigned char c) {
    if (c < 128)
        return ascii_char_classes[c];
    /* Non-ASCII: treat as lower letter */
    return CHAR_LOWER;
}

static inline int16_t max16(int16_t a, int16_t b) {
    return a > b ? a : b;
}

static inline int16_t max16_3(int16_t a, int16_t b, int16_t c) {
    return max16(a, max16(b, c));
}

/* --- Public API --- */

int fzf_has_match(const char *needle, const char *haystack) {
    if (!needle || !needle[0])
        return 1;
    if (!haystack)
        return 0;

    for (const char *np = needle; *np; np++) {
        char nc = tolower((unsigned char)*np);
        while (*haystack) {
            if (tolower((unsigned char)*haystack) == nc) {
                haystack++;
                goto next_char;
            }
            haystack++;
        }
        return 0;
next_char:;
    }
    return 1;
}

/*
 * FuzzyMatchV2 port - the core algorithm.
 *
 * This is a modified Smith-Waterman that finds the optimal fuzzy match.
 * Key property: scores are floored at 0, so long strings don't accumulate
 * deeply negative gap penalties.
 */
score_t fzf_fuzzy_match(const char *needle, const char *haystack) {
    fzf_init();

    if (!needle || !haystack)
        return SCORE_MIN;

    int M = (int)strlen(needle);
    if (M == 0)
        return 0;

    int N = (int)strlen(haystack);
    if (M > N)
        return SCORE_MIN;

    if (N > FZF_MAX_LEN || M > FZF_MAX_LEN)
        return SCORE_MIN;

    /*
     * Phase 1: Quick subsequence check + find first/last occurrence indices.
     * Also lowercase the text and needle into working buffers.
     */
    char lower_needle[FZF_MAX_LEN];
    for (int i = 0; i < M; i++)
        lower_needle[i] = tolower((unsigned char)needle[i]);

    char T[FZF_MAX_LEN];       /* lowercased haystack */
    int16_t B[FZF_MAX_LEN];    /* bonus for each position */
    int16_t H0[FZF_MAX_LEN];   /* score matrix row 0 */
    int16_t C0[FZF_MAX_LEN];   /* consecutive count row 0 */
    int F[FZF_MAX_LEN];        /* first occurrence of each pattern char */

    /*
     * Phase 2: Calculate bonus for each position, find first occurrences,
     * and fill in row 0 of the score matrix.
     */
    int pidx = 0, last_idx = 0;
    char pchar0 = lower_needle[0];
    char pchar = pchar0;
    int16_t prev_H0 = 0;
    char_class_t prev_class = INITIAL_CHAR_CLASS;
    int in_gap = 0;
    int16_t max_score = 0;

    for (int i = 0; i < N; i++) {
        unsigned char uc = (unsigned char)haystack[i];
        char_class_t class = char_class_of(uc);
        char ch = tolower(uc);

        T[i] = ch;
        B[i] = bonus_matrix[prev_class][class];
        prev_class = class;

        if (ch == pchar) {
            if (pidx < M) {
                F[pidx] = i;
                pidx++;
                pchar = (pidx < M) ? lower_needle[pidx] : 0;
            }
            last_idx = i;
        }

        if (ch == pchar0) {
            int16_t score = SCORE_MATCH + B[i] * BONUS_FIRST_CHAR_MULTIPLIER;
            H0[i] = score;
            C0[i] = 1;
            if (M == 1 && score > max_score) {
                max_score = score;
                if (B[i] >= BONUS_BOUNDARY)
                    break;
            }
            in_gap = 0;
        } else {
            if (in_gap) {
                H0[i] = max16(prev_H0 + SCORE_GAP_EXTENSION, 0);
            } else {
                H0[i] = max16(prev_H0 + SCORE_GAP_START, 0);
            }
            C0[i] = 0;
            in_gap = 1;
        }
        prev_H0 = H0[i];
    }

    /* No match if we didn't find all pattern characters */
    if (pidx != M)
        return SCORE_MIN;

    /* Single character pattern: done */
    if (M == 1)
        return (score_t)max_score;

    /*
     * Phase 3: Fill in the score matrix (H) for remaining pattern rows.
     *
     * We only need to consider positions from F[0] to last_idx.
     * The matrix is logically M rows x width columns.
     */
    int f0 = F[0];
    int width = last_idx - f0 + 1;

    /* Allocate the DP matrices on the stack if small enough, else heap */
    int16_t H_static[FZF_MAX_LEN * 32];  /* good for patterns up to 32 chars */
    int16_t C_static[FZF_MAX_LEN * 32];
    int16_t *H, *C;
    int heap_alloc = 0;

    if (M * width <= FZF_MAX_LEN * 32) {
        H = H_static;
        C = C_static;
    } else {
        H = (int16_t *)malloc(sizeof(int16_t) * M * width);
        C = (int16_t *)malloc(sizeof(int16_t) * M * width);
        if (!H || !C) {
            free(H);
            free(C);
            return SCORE_MIN;
        }
        heap_alloc = 1;
    }

    /* Copy row 0 from H0/C0 */
    for (int j = 0; j < width; j++) {
        H[j] = H0[f0 + j];
        C[j] = C0[f0 + j];
    }

    /* Fill remaining rows */
    for (int i = 1; i < M; i++) {
        int f = F[i];
        char pc = lower_needle[i];
        int row = i * width;
        in_gap = 0;

        /* Zero out positions before F[i] in this row, and set
         * the left boundary to 0 (equivalent to fzf's Hleft[0] = 0) */
        for (int off = 0; off < f - f0; off++) {
            H[row + off] = 0;
            C[row + off] = 0;
        }

        for (int off = f - f0; off < width; off++) {
            int col = off + f0;
            int16_t s1 = 0, s2 = 0;
            int16_t consecutive = 0;

            /* s2: gap score (extending from left) */
            if (off > 0) {
                if (in_gap) {
                    s2 = H[row + off - 1] + SCORE_GAP_EXTENSION;
                } else {
                    s2 = H[row + off - 1] + SCORE_GAP_START;
                }
            }

            /* s1: match score (diagonal) */
            if (T[col] == pc) {
                if (off > 0) {
                    s1 = H[(i - 1) * width + off - 1] + SCORE_MATCH;
                    int16_t b = B[col];
                    consecutive = C[(i - 1) * width + off - 1] + 1;
                    if (consecutive > 1) {
                        int16_t fb = B[col - (int)consecutive + 1];
                        if (b >= BONUS_BOUNDARY && b > fb) {
                            consecutive = 1;
                        } else {
                            b = max16_3(b, BONUS_CONSECUTIVE, fb);
                        }
                    }
                    if (s1 + b < s2) {
                        s1 += B[col];
                        consecutive = 0;
                    } else {
                        s1 += b;
                    }
                }
            }

            C[row + off] = consecutive;
            in_gap = (s1 < s2);

            int16_t score = max16_3(s1, s2, 0);
            if (i == M - 1 && score > max_score) {
                max_score = score;
            }
            H[row + off] = score;
        }
    }

    if (heap_alloc) {
        free(H);
        free(C);
    }

    return (score_t)max_score;
}
