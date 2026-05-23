#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/cofi_json_io.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(name, cond) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("PASS: %s\n", name); \
    } else { \
        printf("FAIL: %s (line %d)\n", name, __LINE__); \
    } \
} while (0)

static void set_test_home(const char *suffix) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-json-io-%s-%ld", suffix, (long)getpid());
    mkdir(path, 0755);
    setenv("HOME", path, 1);
}

static void write_file(const char *path, const char *content) {
    FILE *file = fopen(path, "w");
    if (!file) {
        return;
    }
    fputs(content, file);
    fclose(file);
}

static void test_load_returns_null_on_missing_file(void) {
    set_test_home("missing");
    JsonParser *parser = cofi_json_load_object_file("/tmp/cofi-json-io-missing-nope.json");
    ASSERT_TRUE("missing file returns null parser", parser == NULL);
}

static void test_load_returns_null_on_empty_file(void) {
    set_test_home("empty");
    const char *path = "/tmp/cofi-json-io-empty.json";
    write_file(path, "");
    JsonParser *parser = cofi_json_load_object_file(path);
    ASSERT_TRUE("empty file returns null parser", parser == NULL);
}

static void test_load_returns_null_on_corrupt_json(void) {
    set_test_home("corrupt");
    const char *path = "/tmp/cofi-json-io-corrupt.json";
    write_file(path, "{\"foo\":");
    JsonParser *parser = cofi_json_load_object_file(path);
    ASSERT_TRUE("corrupt file returns null parser", parser == NULL);
}

static void test_load_returns_null_on_non_object_root(void) {
    set_test_home("array-root");
    const char *path = "/tmp/cofi-json-io-array.json";
    write_file(path, "[1, 2]");
    JsonParser *parser = cofi_json_load_object_file(path);
    ASSERT_TRUE("array root returns null parser", parser == NULL);
}

static void test_obj_str_or_returns_default_on_missing_key(void) {
    JsonParser *p = json_parser_new();
    ASSERT_TRUE("parser loads object fixture",
                json_parser_load_from_data(p, "{\"ok\":\"yes\"}", -1, NULL));
    JsonObject *obj = json_node_get_object(json_parser_get_root(p));
    gboolean present = TRUE;
    const char *value = cofi_json_obj_str_or(obj, "missing", "fallback", &present);
    ASSERT_TRUE("missing str key returns fallback", strcmp(value, "fallback") == 0);
    ASSERT_TRUE("missing str key sets present false", present == FALSE);
    g_object_unref(p);
}

static void test_obj_str_or_returns_default_on_null_value(void) {
    JsonParser *p = json_parser_new();
    ASSERT_TRUE("parser loads null str fixture",
                json_parser_load_from_data(p, "{\"name\":null}", -1, NULL));
    JsonObject *obj = json_node_get_object(json_parser_get_root(p));
    gboolean present = TRUE;
    const char *value = cofi_json_obj_str_or(obj, "name", "fallback", &present);
    ASSERT_TRUE("null str key returns fallback", strcmp(value, "fallback") == 0);
    ASSERT_TRUE("null str key sets present false", present == FALSE);
    g_object_unref(p);
}

static void test_obj_str_or_returns_default_on_wrong_type(void) {
    JsonParser *p = json_parser_new();
    ASSERT_TRUE("parser loads wrong-type str fixture",
                json_parser_load_from_data(p, "{\"name\":12}", -1, NULL));
    JsonObject *obj = json_node_get_object(json_parser_get_root(p));
    gboolean present = TRUE;
    const char *value = cofi_json_obj_str_or(obj, "name", "fallback", &present);
    ASSERT_TRUE("wrong-type str key returns fallback", strcmp(value, "fallback") == 0);
    ASSERT_TRUE("wrong-type str key sets present false", present == FALSE);
    g_object_unref(p);
}

static void test_obj_int_or_returns_default_on_wrong_type(void) {
    JsonParser *p = json_parser_new();
    ASSERT_TRUE("parser loads wrong-type int fixture",
                json_parser_load_from_data(p, "{\"n\":\"12\"}", -1, NULL));
    JsonObject *obj = json_node_get_object(json_parser_get_root(p));
    gboolean present = TRUE;
    int value = cofi_json_obj_int_or(obj, "n", 42, &present);
    ASSERT_TRUE("wrong-type int key returns fallback", value == 42);
    ASSERT_TRUE("wrong-type int key sets present false", present == FALSE);
    g_object_unref(p);
}

static void test_obj_bool_or_handles_true_false_missing(void) {
    JsonParser *p = json_parser_new();
    ASSERT_TRUE("parser loads bool fixture",
                json_parser_load_from_data(p, "{\"a\":true,\"b\":false}", -1, NULL));
    JsonObject *obj = json_node_get_object(json_parser_get_root(p));

    gboolean present = FALSE;
    gboolean a = cofi_json_obj_bool_or(obj, "a", FALSE, &present);
    ASSERT_TRUE("bool true reads true", a == TRUE);
    ASSERT_TRUE("bool true marks present", present == TRUE);

    present = TRUE;
    gboolean b = cofi_json_obj_bool_or(obj, "b", TRUE, &present);
    ASSERT_TRUE("bool false reads false", b == FALSE);
    ASSERT_TRUE("bool false marks present", present == TRUE);

    present = TRUE;
    gboolean c = cofi_json_obj_bool_or(obj, "missing", TRUE, &present);
    ASSERT_TRUE("missing bool returns fallback", c == TRUE);
    ASSERT_TRUE("missing bool marks absent", present == FALSE);
    g_object_unref(p);
}

static void test_obj_array_returns_null_on_missing_or_wrong_type(void) {
    JsonParser *p = json_parser_new();
    ASSERT_TRUE("parser loads array fixture",
                json_parser_load_from_data(p, "{\"a\":[1],\"b\":3}", -1, NULL));
    JsonObject *obj = json_node_get_object(json_parser_get_root(p));
    ASSERT_TRUE("missing array key returns null", cofi_json_obj_array(obj, "missing") == NULL);
    ASSERT_TRUE("wrong-type array key returns null", cofi_json_obj_array(obj, "b") == NULL);
    ASSERT_TRUE("valid array key returns array", cofi_json_obj_array(obj, "a") != NULL);
    g_object_unref(p);
}

static void test_obj_object_returns_null_on_missing_or_wrong_type(void) {
    JsonParser *p = json_parser_new();
    ASSERT_TRUE("parser loads object fixture",
                json_parser_load_from_data(p, "{\"a\":{\"x\":1},\"b\":3}", -1, NULL));
    JsonObject *obj = json_node_get_object(json_parser_get_root(p));
    ASSERT_TRUE("missing object key returns null", cofi_json_obj_object(obj, "missing") == NULL);
    ASSERT_TRUE("wrong-type object key returns null", cofi_json_obj_object(obj, "b") == NULL);
    ASSERT_TRUE("valid object key returns object", cofi_json_obj_object(obj, "a") != NULL);
    g_object_unref(p);
}

static void test_save_then_load_roundtrip_preserves_object(void) {
    set_test_home("roundtrip");
    const char *path = "/tmp/cofi-json-io-roundtrip.json";

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "name");
    json_builder_add_string_value(builder, "cofi");
    json_builder_set_member_name(builder, "count");
    json_builder_add_int_value(builder, 7);
    json_builder_end_object(builder);
    JsonNode *root = json_builder_get_root(builder);

    ASSERT_TRUE("save_root succeeds", cofi_json_save_root(path, root));

    json_node_unref(root);
    g_object_unref(builder);

    JsonParser *parser = cofi_json_load_object_file(path);
    ASSERT_TRUE("load after save returns parser", parser != NULL);
    if (parser) {
        JsonObject *obj = json_node_get_object(json_parser_get_root(parser));
        ASSERT_TRUE("roundtrip string preserved",
                    strcmp(cofi_json_obj_str_or(obj, "name", "", NULL), "cofi") == 0);
        ASSERT_TRUE("roundtrip int preserved",
                    cofi_json_obj_int_or(obj, "count", 0, NULL) == 7);
        g_object_unref(parser);
    }
}

static void test_save_success_leaves_no_tmp_file(void) {
    set_test_home("tmp");
    const char *path = "/tmp/cofi-json-io-tmp.json";
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "ok");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_end_object(builder);
    JsonNode *root = json_builder_get_root(builder);

    ASSERT_TRUE("save_root writes target file", cofi_json_save_root(path, root));
    ASSERT_TRUE("save_root removes tmp file", access(tmp_path, F_OK) != 0 && errno == ENOENT);

    json_node_unref(root);
    g_object_unref(builder);
}

static void test_save_failure_leaves_target_unchanged(void) {
    set_test_home("save-fail");
    const char *path = "/tmp/cofi-json-io-save-fail.json";
    const char *before = "{\"old\":true}\n";
    write_file(path, before);

    char bad_target[256];
    snprintf(bad_target, sizeof(bad_target), "%s.dir", path);
    mkdir(bad_target, 0755);

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "new");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_end_object(builder);
    JsonNode *root = json_builder_get_root(builder);

    ASSERT_TRUE("save_root fails when target is directory path",
                !cofi_json_save_root(bad_target, root));

    char *after = NULL;
    gsize len = 0;
    ASSERT_TRUE("existing file remains readable",
                g_file_get_contents(path, &after, &len, NULL));
    ASSERT_TRUE("existing file content unchanged",
                after && strcmp(after, before) == 0);
    g_free(after);

    json_node_unref(root);
    g_object_unref(builder);
}

static void test_unknown_field_ignored_silently(void) {
    JsonParser *p = json_parser_new();
    ASSERT_TRUE("parser loads unknown key fixture",
                json_parser_load_from_data(p, "{\"known\":1,\"unknown\":2}", -1, NULL));
    JsonObject *obj = json_node_get_object(json_parser_get_root(p));
    ASSERT_TRUE("known field still readable", cofi_json_obj_int_or(obj, "known", 0, NULL) == 1);
    ASSERT_TRUE("unknown field access not required", cofi_json_obj_int_or(obj, "missing", 5, NULL) == 5);
    g_object_unref(p);
}

int main(void) {
    printf("cofi_json_io tests\n");
    printf("==================\n\n");

    test_load_returns_null_on_missing_file();
    test_load_returns_null_on_empty_file();
    test_load_returns_null_on_corrupt_json();
    test_load_returns_null_on_non_object_root();
    test_obj_str_or_returns_default_on_missing_key();
    test_obj_str_or_returns_default_on_null_value();
    test_obj_str_or_returns_default_on_wrong_type();
    test_obj_int_or_returns_default_on_wrong_type();
    test_obj_bool_or_handles_true_false_missing();
    test_obj_array_returns_null_on_missing_or_wrong_type();
    test_obj_object_returns_null_on_missing_or_wrong_type();
    test_save_then_load_roundtrip_preserves_object();
    test_save_success_leaves_no_tmp_file();
    test_save_failure_leaves_target_unchanged();
    test_unknown_field_ignored_silently();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
