#ifndef CALC_H
#define CALC_H

#include <glib.h>

#define CALC_HISTORY_CAP 32
#define CALC_RESULT_LEN  64
#define CALC_EXPR_LEN    256

typedef struct {
    char result[CALC_RESULT_LEN];
    char expr[CALC_EXPR_LEN];
} CalcEntry;

typedef struct {
    CalcEntry entries[CALC_HISTORY_CAP];
    int count;
    char last_result[CALC_RESULT_LEN];   /* last numeric result, empty if last was error */
} CalcMode;

/* Evaluate raw_expr (after stripping leading '='). If starts with operator,
 * prepend last_result. Pushes to history. Returns TRUE on success.
 * result_out receives the formatted result or error string. */
gboolean calc_eval(CalcMode *calc, const char *raw_expr, char *result_out);

/* Prepend a new entry (newest-first) onto the history ring. */
void calc_push(CalcMode *calc, const char *expr, const char *result, gboolean is_error);

/* Format double to string, trimming trailing zeros; integers shown as integers. */
void calc_format_double(double val, char *out, int out_len);

/* Strip leading spaces, one optional calc prefix '=', and numeric underscores.
 * If the remaining raw expression starts with an operator and last_result is
 * set, prepend it. */
void calc_prepare_expr(CalcMode *calc, const char *raw, char *out, int out_len);

#endif /* CALC_H */
