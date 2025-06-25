#ifndef MATCH_H
#define MATCH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef double score_t;
#define SCORE_MAX 1e308
#define SCORE_MIN -1e308

#define MATCH_MAX_LEN 1024

int has_match(const char *needle, const char *haystack);
score_t match_positions(const char *needle, const char *haystack, size_t *positions);
score_t match(const char *needle, const char *haystack);

#ifdef __cplusplus
}
#endif

#endif
