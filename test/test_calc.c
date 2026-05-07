/*
 * Behavioral tests for calc.c
 *
 * Covers:
 *   - calc_format_double: integer result, floating, trailing zeros
 *   - calc_prepare_expr: operator-continuation prepends last_result
 *   - calc_eval: basic arithmetic, error case
 *   - history push: oldest-first (newest-last), cap, last_result tracking
 *   - operator-continuation end-to-end through calc_eval
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

/* Stub log functions that calc.c calls */
void log_debug(const char *fmt, ...) { (void)fmt; }
void log_info(const char *fmt, ...) { (void)fmt; }
void log_warn(const char *fmt, ...) { (void)fmt; }

#include "../src/calc.c"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else       { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#define ASSERT_STR(name, a, b) \
    ASSERT_TRUE(name, strcmp((a), (b)) == 0)

/* ---- calc_format_double ---- */

static void test_format_integer(void) {
    char out[CALC_RESULT_LEN];
    calc_format_double(42.0, out, sizeof(out));
    ASSERT_STR("format integer 42", out, "42");
}

static void test_format_negative_integer(void) {
    char out[CALC_RESULT_LEN];
    calc_format_double(-7.0, out, sizeof(out));
    ASSERT_STR("format integer -7", out, "-7");
}

static void test_format_zero(void) {
    char out[CALC_RESULT_LEN];
    calc_format_double(0.0, out, sizeof(out));
    ASSERT_STR("format zero", out, "0");
}

static void test_format_float_trims_zeros(void) {
    char out[CALC_RESULT_LEN];
    calc_format_double(1.5, out, sizeof(out));
    ASSERT_STR("format 1.5", out, "1.5");
}

static void test_format_does_not_show_trailing_zeros(void) {
    char out[CALC_RESULT_LEN];
    /* 1.0/3.0 should NOT end in zeros; just check it's not "0.3333333330" */
    calc_format_double(1.0/3.0, out, sizeof(out));
    int len = strlen(out);
    ASSERT_TRUE("format 1/3 no trailing zeros",
                len > 0 && out[len - 1] != '0');
}

/* ---- calc_prepare_expr ---- */

static void test_prepare_plain_expression(void) {
    CalcMode calc = {0};
    char out[CALC_EXPR_LEN];
    calc_prepare_expr(&calc, "17+4", out, sizeof(out));
    ASSERT_STR("prepare plain expr unchanged", out, "17+4");
}

static void test_prepare_operator_with_last_result_prepends(void) {
    CalcMode calc = {0};
    strncpy(calc.last_result, "21", sizeof(calc.last_result) - 1);
    char out[CALC_EXPR_LEN];
    calc_prepare_expr(&calc, "*2", out, sizeof(out));
    ASSERT_STR("prepare *2 prepends last_result", out, "21*2");
}

static void test_prepare_operator_without_last_result_unchanged(void) {
    CalcMode calc = {0};
    /* last_result is empty */
    char out[CALC_EXPR_LEN];
    calc_prepare_expr(&calc, "*2", out, sizeof(out));
    ASSERT_STR("prepare *2 with no last_result unchanged", out, "*2");
}

static void test_prepare_leading_whitespace_stripped(void) {
    CalcMode calc = {0};
    strncpy(calc.last_result, "10", sizeof(calc.last_result) - 1);
    char out[CALC_EXPR_LEN];
    calc_prepare_expr(&calc, "  +5", out, sizeof(out));
    ASSERT_STR("prepare strips leading whitespace before operator", out, "10+5");
}

/* ---- calc_push / history ---- */

static void test_push_newest_last(void) {
    CalcMode calc = {0};
    calc_push(&calc, "1+1", "2", FALSE);
    calc_push(&calc, "2*2", "4", FALSE);
    ASSERT_STR("history[0] is oldest", calc.entries[0].result, "2");
    ASSERT_STR("history[1] is newest", calc.entries[1].result, "4");
    ASSERT_TRUE("count is 2", calc.count == 2);
}

static void test_push_updates_last_result_on_success(void) {
    CalcMode calc = {0};
    calc_push(&calc, "3+3", "6", FALSE);
    ASSERT_STR("last_result updated", calc.last_result, "6");
}

static void test_push_error_does_not_update_last_result(void) {
    CalcMode calc = {0};
    strncpy(calc.last_result, "99", sizeof(calc.last_result) - 1);
    calc_push(&calc, "bad", "[error: bad expression]", TRUE);
    ASSERT_STR("last_result preserved on error", calc.last_result, "99");
}

static void test_push_respects_history_cap(void) {
    CalcMode calc = {0};
    char expr[16], result[16];
    for (int i = 0; i < CALC_HISTORY_CAP + 5; i++) {
        snprintf(expr, sizeof(expr), "%d", i);
        snprintf(result, sizeof(result), "%d", i);
        calc_push(&calc, expr, result, FALSE);
    }
    ASSERT_TRUE("history capped at CALC_HISTORY_CAP", calc.count == CALC_HISTORY_CAP);
    /* Newest entry should be the last one pushed, at the tail */
    char newest[16];
    snprintf(newest, sizeof(newest), "%d", CALC_HISTORY_CAP + 4);
    ASSERT_STR("newest entry is last pushed", calc.entries[CALC_HISTORY_CAP - 1].result, newest);
}

/* ---- calc_eval ---- */

static void test_eval_basic_addition(void) {
    CalcMode calc = {0};
    char result[CALC_RESULT_LEN];
    gboolean ok = calc_eval(&calc, "17+4", result);
    ASSERT_TRUE("eval 17+4 succeeds", ok);
    ASSERT_STR("eval 17+4 = 21", result, "21");
}

static void test_eval_result_pushed_to_history(void) {
    CalcMode calc = {0};
    char result[CALC_RESULT_LEN];
    calc_eval(&calc, "5*5", result);
    ASSERT_TRUE("history count 1 after eval", calc.count == 1);
    ASSERT_STR("history entry result", calc.entries[0].result, "25");
}

static void test_eval_sets_last_result(void) {
    CalcMode calc = {0};
    char result[CALC_RESULT_LEN];
    calc_eval(&calc, "10/2", result);
    ASSERT_STR("last_result set after eval", calc.last_result, "5");
}

static void test_eval_error_returns_false(void) {
    CalcMode calc = {0};
    char result[CALC_RESULT_LEN];
    gboolean ok = calc_eval(&calc, "1++bad", result);
    ASSERT_TRUE("eval bad expr returns FALSE", !ok);
    ASSERT_TRUE("error message in result", strstr(result, "error") != NULL);
}

static void test_eval_error_still_pushed_to_history(void) {
    CalcMode calc = {0};
    char result[CALC_RESULT_LEN];
    calc_eval(&calc, "??", result);
    ASSERT_TRUE("error entry pushed to history", calc.count == 1);
}

static void test_eval_operator_continuation(void) {
    CalcMode calc = {0};
    char result[CALC_RESULT_LEN];
    calc_eval(&calc, "17+4", result);  /* result = 21, last_result = "21" */
    calc_eval(&calc, "*2", result);    /* should prepend "21" → "21*2" = 42 */
    ASSERT_STR("operator continuation: *2 after 21 = 42", result, "42");
    ASSERT_STR("history[0] result is 21 (oldest)", calc.entries[0].result, "21");
    ASSERT_STR("history[1] result is 42 (newest)", calc.entries[1].result, "42");
}

static void test_eval_empty_expr_returns_false(void) {
    CalcMode calc = {0};
    char result[CALC_RESULT_LEN];
    gboolean ok = calc_eval(&calc, "", result);
    ASSERT_TRUE("empty expr returns FALSE", !ok);
    ASSERT_TRUE("empty expr does not push to history", calc.count == 0);
}

static void test_eval_chained_operations(void) {
    CalcMode calc = {0};
    char result[CALC_RESULT_LEN];
    calc_eval(&calc, "10", result);   /* 10 */
    calc_eval(&calc, "+5", result);   /* 10+5 = 15 */
    calc_eval(&calc, "*3", result);   /* 15*3 = 45 */
    ASSERT_STR("chained: 10 +5 *3 = 45", result, "45");
    ASSERT_TRUE("history has 3 entries", calc.count == 3);
}

/* ---- main ---- */

int main(void) {
    printf("Calculator behavioral tests\n");
    printf("===========================\n\n");

    test_format_integer();
    test_format_negative_integer();
    test_format_zero();
    test_format_float_trims_zeros();
    test_format_does_not_show_trailing_zeros();

    test_prepare_plain_expression();
    test_prepare_operator_with_last_result_prepends();
    test_prepare_operator_without_last_result_unchanged();
    test_prepare_leading_whitespace_stripped();

    test_push_newest_last();
    test_push_updates_last_result_on_success();
    test_push_error_does_not_update_last_result();
    test_push_respects_history_cap();

    test_eval_basic_addition();
    test_eval_result_pushed_to_history();
    test_eval_sets_last_result();
    test_eval_error_returns_false();
    test_eval_error_still_pushed_to_history();
    test_eval_operator_continuation();
    test_eval_empty_expr_returns_false();
    test_eval_chained_operations();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return (fail == 0) ? 0 : 1;
}
