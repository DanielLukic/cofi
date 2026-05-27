#ifndef UTF8_COLUMNS_H
#define UTF8_COLUMNS_H

#include <stddef.h>

#include <glib.h>

int utf8_cluster_columns(const char *cluster, size_t len);
const char *utf8_next_cluster(const char *p, const char *end);
int utf8_text_columns(const char *text);
void utf8_clean_text(const char *text, char *output, size_t output_size);
void utf8_fit_columns(const char *text, int width, char *output, size_t output_size);
void utf8_fit_columns_aligned(const char *text, int width, int align_right,
                              char *output, size_t output_size);
void utf8_fit_columns_ellipsis(const char *text, int width, char *output, size_t output_size);
void utf8_clip_lines_to_columns(GString *text, int target_columns);

#endif /* UTF8_COLUMNS_H */
