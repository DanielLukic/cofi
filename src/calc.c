#include "calc.h"
#include "tinyexpr.h"
#include "log.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <locale.h>

static int is_leading_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^';
}

static char *push_c_numeric_locale(void) {
    const char *current = setlocale(LC_NUMERIC, NULL);
    char *saved = current ? g_strdup(current) : NULL;
    setlocale(LC_NUMERIC, "C");
    return saved;
}

static void pop_numeric_locale(char *saved) {
    if (saved) {
        setlocale(LC_NUMERIC, saved);
        g_free(saved);
    }
}

static void remove_digit_separators(char *expr) {
    if (!expr) return;

    char *write = expr;
    for (char *read = expr; *read; read++) {
        if (*read == '_' &&
            read > expr &&
            read[1] != '\0' &&
            isdigit((unsigned char)read[-1]) &&
            isdigit((unsigned char)read[1])) {
            continue;
        }
        *write++ = *read;
    }
    *write = '\0';
}

static gboolean commas_are_valid(const char *expr) {
    gboolean function_parens[CALC_EXPR_LEN] = {0};
    int depth = 0;

    if (!expr) return FALSE;

    for (const char *p = expr; *p; p++) {
        if (*p == '(') {
            const char *q = p;
            while (q > expr && isspace((unsigned char)q[-1])) q--;
            const char *name_start = q;
            while (name_start > expr &&
                   (isalnum((unsigned char)name_start[-1]) || name_start[-1] == '_')) {
                name_start--;
            }
            gboolean is_function = name_start < q &&
                (isalpha((unsigned char)*name_start) || *name_start == '_');
            if (depth < CALC_EXPR_LEN) {
                function_parens[depth] = is_function;
            }
            depth++;
        } else if (*p == ')') {
            if (depth > 0) depth--;
        } else if (*p == ',') {
            if (depth <= 0) return FALSE;
            int paren_idx = depth - 1;
            if (paren_idx >= CALC_EXPR_LEN || !function_parens[paren_idx]) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

void calc_format_double(double val, char *out, int out_len) {
    if (!out || out_len <= 0) return;
    if (isnan(val) || isinf(val)) {
        snprintf(out, out_len, "error");
        return;
    }
    /* Show as integer when lossless */
    char *saved_numeric_locale = push_c_numeric_locale();
    if (val == floor(val) && fabs(val) < 1e15) {
        snprintf(out, out_len, "%lld", (long long)val);
        pop_numeric_locale(saved_numeric_locale);
        return;
    }
    /* Use %g to trim trailing zeros; 10 significant digits */
    snprintf(out, out_len, "%.10g", val);
    pop_numeric_locale(saved_numeric_locale);
}

void calc_prepare_expr(CalcMode *calc, const char *raw, char *out, int out_len) {
    if (!calc || !raw || !out || out_len <= 0) return;

    const char *p = raw;
    while (*p == ' ') p++;
    if (*p == '=') p++;
    while (*p == ' ') p++;

    char prepared[CALC_EXPR_LEN];
    if (*p != '\0' && is_leading_operator(*p) && calc->last_result[0] != '\0') {
        snprintf(prepared, sizeof(prepared), "%s%s", calc->last_result, p);
    } else {
        snprintf(prepared, sizeof(prepared), "%s", p);
    }
    remove_digit_separators(prepared);
    snprintf(out, out_len, "%s", prepared);
}

void calc_push(CalcMode *calc, const char *expr, const char *result, gboolean is_error) {
    if (!calc || !expr || !result) return;

    if (calc->count < CALC_HISTORY_CAP)
        calc->count++;

    for (int i = calc->count - 1; i > 0; i--) {
        calc->entries[i] = calc->entries[i - 1];
    }

    strncpy(calc->entries[0].expr, expr, CALC_EXPR_LEN - 1);
    calc->entries[0].expr[CALC_EXPR_LEN - 1] = '\0';
    strncpy(calc->entries[0].result, result, CALC_RESULT_LEN - 1);
    calc->entries[0].result[CALC_RESULT_LEN - 1] = '\0';

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
    if (!commas_are_valid(prepared)) {
        snprintf(result_out, CALC_RESULT_LEN, "[error: bad expression]");
        calc_push(calc, prepared, result_out, TRUE);
        log_debug("calc: eval error for '%s'", prepared);
        return FALSE;
    }

    int error = 0;
    char *saved_numeric_locale = push_c_numeric_locale();
    double val = te_interp(prepared, &error);
    pop_numeric_locale(saved_numeric_locale);

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
