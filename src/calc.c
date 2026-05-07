#include "calc.h"
#include "tinyexpr.h"
#include "log.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int is_leading_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^';
}

void calc_format_double(double val, char *out, int out_len) {
    if (!out || out_len <= 0) return;
    if (isnan(val) || isinf(val)) {
        snprintf(out, out_len, "error");
        return;
    }
    /* Show as integer when lossless */
    if (val == floor(val) && fabs(val) < 1e15) {
        snprintf(out, out_len, "%lld", (long long)val);
        return;
    }
    /* Use %g to trim trailing zeros; 10 significant digits */
    snprintf(out, out_len, "%.10g", val);
}

void calc_prepare_expr(CalcMode *calc, const char *raw, char *out, int out_len) {
    if (!calc || !raw || !out || out_len <= 0) return;

    const char *p = raw;
    while (*p == ' ') p++;

    if (*p != '\0' && is_leading_operator(*p) && calc->last_result[0] != '\0') {
        snprintf(out, out_len, "%s%s", calc->last_result, p);
    } else {
        snprintf(out, out_len, "%s", p);
    }
}

void calc_push(CalcMode *calc, const char *expr, const char *result, gboolean is_error) {
    if (!calc || !expr || !result) return;

    int idx;
    if (calc->count < CALC_HISTORY_CAP) {
        idx = calc->count++;
    } else {
        for (int i = 0; i < CALC_HISTORY_CAP - 1; i++)
            calc->entries[i] = calc->entries[i + 1];
        idx = CALC_HISTORY_CAP - 1;
    }
    strncpy(calc->entries[idx].expr, expr, CALC_EXPR_LEN - 1);
    calc->entries[idx].expr[CALC_EXPR_LEN - 1] = '\0';
    strncpy(calc->entries[idx].result, result, CALC_RESULT_LEN - 1);
    calc->entries[idx].result[CALC_RESULT_LEN - 1] = '\0';

    if (!is_error) {
        strncpy(calc->last_result, result, CALC_RESULT_LEN - 1);
        calc->last_result[CALC_RESULT_LEN - 1] = '\0';
    }
}

gboolean calc_eval(CalcMode *calc, const char *raw_expr, char *result_out) {
    if (!calc || !raw_expr || !result_out) return FALSE;

    char prepared[CALC_EXPR_LEN];
    calc_prepare_expr(calc, raw_expr, prepared, sizeof(prepared));

    if (prepared[0] == '\0') return FALSE;

    int error = 0;
    double val = te_interp(prepared, &error);

    if (error || isnan(val) || isinf(val)) {
        snprintf(result_out, CALC_RESULT_LEN, "[error: bad expression]");
        calc_push(calc, prepared, result_out, TRUE);
        log_debug("calc: eval error for '%s'", prepared);
        return FALSE;
    }

    calc_format_double(val, result_out, CALC_RESULT_LEN);
    calc_push(calc, prepared, result_out, FALSE);
    log_debug("calc: '%s' = %s", prepared, result_out);
    return TRUE;
}
