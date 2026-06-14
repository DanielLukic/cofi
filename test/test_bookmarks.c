#include <stdio.h>
#include <string.h>

#include "bookmarks/bookmarks.h"

/* Stub log_log and detach_launch_argv_array so the test binary links without
 * the daemon/log objects (we exercise the parser/filter only). */
void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}
gboolean detach_launch_argv_array(const char *const *argv) {
    (void)argv;
    return TRUE;
}

static int tests_run = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(name, cond) do { \
    tests_run++; \
    if (cond) printf("PASS: %s\n", name); \
    else { printf("FAIL: %s\n", name); tests_failed++; } \
} while (0)

static const char *kSampleJson =
    "{"
    "  \"roots\": {"
    "    \"bookmark_bar\": {"
    "      \"name\": \"Bookmarks bar\","
    "      \"type\": \"folder\","
    "      \"children\": ["
    "        {\"type\": \"url\", \"name\": \"Top Site\","
    "         \"url\": \"https://top.example.test/path\"},"
    "        {\"type\": \"folder\", \"name\": \"Docs\","
    "         \"children\": ["
    "           {\"type\": \"url\", \"name\": \"Nested Page\","
    "            \"url\": \"https://docs.example.test/intro\"},"
    "           {\"type\": \"folder\", \"name\": \"Deep\","
    "            \"children\": ["
    "              {\"type\": \"url\", \"name\": \"Very Deep\","
    "               \"url\": \"https://deep.example.test/a:b:c\"}"
    "            ]}"
    "         ]}"
    "      ]"
    "    },"
    "    \"other\": {"
    "      \"type\": \"folder\","
    "      \"children\": ["
    "        {\"type\": \"url\", \"name\": \"Other One\","
    "         \"url\": \"https://other.example.test/\"},"
    "        {\"type\": \"url\", \"name\": \"\","
    "         \"url\": \"https://fallback.example.test/x\"}"
    "      ]"
    "    },"
    "    \"synced\": {"
    "      \"type\": \"folder\","
    "      \"children\": []"
    "    }"
    "  },"
    "  \"checksum\": \"abc123\","
    "  \"sync_transaction_version\": \"42\""
    "}";

static const char *kSchemeJson =
    "{\"roots\": {"
    "  \"bookmark_bar\": {\"type\":\"folder\",\"children\": ["
    "    {\"type\":\"url\",\"name\":\"http ok\",\"url\":\"http://a.example/\"},"
    "    {\"type\":\"url\",\"name\":\"https ok\",\"url\":\"https://b.example/\"},"
    "    {\"type\":\"url\",\"name\":\"ftp ok\",\"url\":\"ftp://c.example/\"},"
    "    {\"type\":\"url\",\"name\":\"file ok\",\"url\":\"file:///etc/hosts\"},"
    "    {\"type\":\"url\",\"name\":\"js bad\",\"url\":\"javascript:alert(1)\"},"
    "    {\"type\":\"url\",\"name\":\"chrome bad\",\"url\":\"chrome://flags\"},"
    "    {\"type\":\"url\",\"name\":\"ext bad\",\"url\":\"chrome-extension://abc/page.html\"},"
    "    {\"type\":\"url\",\"name\":\"data bad\",\"url\":\"data:text/plain,hello\"},"
    "    {\"type\":\"url\",\"name\":\"empty\",\"url\":\"\"}"
    "  ]},"
    "  \"other\":{\"type\":\"folder\",\"children\":[]},"
    "  \"synced\":{\"type\":\"folder\",\"children\":[]}"
    "}}";

static void test_parser_basic(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    char err[256] = {0};
    int n = bookmarks_parse_chrome(&mode, kSampleJson, "Default", "DefaultUser",
                                   err, sizeof(err));
    ASSERT_TRUE("parses five allowed URLs (4 named + 1 url-fallback name)", n == 5);
    ASSERT_TRUE("no parse error", err[0] == '\0');

    BookmarkEntry *e0 = &g_array_index(mode.entries, BookmarkEntry, 0);
    ASSERT_TRUE("first row is top-level", e0->folder_breadcrumb[0] == '\0');
    ASSERT_TRUE("first row name", strcmp(e0->name, "Top Site") == 0);

    BookmarkEntry *e1 = &g_array_index(mode.entries, BookmarkEntry, 1);
    ASSERT_TRUE("nested breadcrumb", strcmp(e1->folder_breadcrumb, "Docs") == 0);

    BookmarkEntry *e2 = &g_array_index(mode.entries, BookmarkEntry, 2);
    ASSERT_TRUE("deep breadcrumb", strcmp(e2->folder_breadcrumb, "Docs > Deep") == 0);
    ASSERT_TRUE("deep url preserves colons",
                strcmp(e2->url, "https://deep.example.test/a:b:c") == 0);

    BookmarkEntry *e3 = &g_array_index(mode.entries, BookmarkEntry, 3);
    ASSERT_TRUE("other-section row picked up",
                strcmp(e3->url, "https://other.example.test/") == 0);
    bookmarks_mode_free(&mode);
}

static void test_parser_name_fallback_to_url(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    const char *json =
        "{\"roots\":{\"bookmark_bar\":{\"type\":\"folder\",\"children\":["
        "  {\"type\":\"url\",\"name\":\"\",\"url\":\"https://name-fallback.example/\"}"
        "]},\"other\":{\"type\":\"folder\",\"children\":[]},"
        "\"synced\":{\"type\":\"folder\",\"children\":[]}}}";
    char err[256] = {0};
    int n = bookmarks_parse_chrome(&mode, json, "Default", "User", err, sizeof(err));
    ASSERT_TRUE("one url parsed", n == 1);
    BookmarkEntry *e = &g_array_index(mode.entries, BookmarkEntry, 0);
    ASSERT_TRUE("empty name falls back to url",
                strcmp(e->name, "https://name-fallback.example/") == 0);
    bookmarks_mode_free(&mode);
}

static void test_parser_profile_label_fallback(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    const char *json = "{\"roots\":{\"bookmark_bar\":{\"type\":\"folder\",\"children\":["
        "{\"type\":\"url\",\"name\":\"X\",\"url\":\"https://x.example/\"}]},"
        "\"other\":{\"type\":\"folder\",\"children\":[]},"
        "\"synced\":{\"type\":\"folder\",\"children\":[]}}}";
    char err[256] = {0};
    (void)bookmarks_parse_chrome(&mode, json, "Profile 7", "", err, sizeof(err));
    BookmarkEntry *e = &g_array_index(mode.entries, BookmarkEntry, 0);
    ASSERT_TRUE("empty profile label falls back to profile_dir",
                strcmp(e->profile_label, "Profile 7") == 0);
    bookmarks_mode_free(&mode);
}

static void test_parser_malformed(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    char err[256] = {0};
    int n = bookmarks_parse_chrome(&mode, "{not json", "Default", "X",
                                   err, sizeof(err));
    ASSERT_TRUE("malformed returns 0", n == 0);
    ASSERT_TRUE("malformed populates error", err[0] != '\0');
    bookmarks_mode_free(&mode);
}

static void test_parser_empty_roots(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    char err[256] = {0};
    int n = bookmarks_parse_chrome(&mode, "{\"roots\":{}}", "Default", "X",
                                   err, sizeof(err));
    ASSERT_TRUE("empty roots returns 0", n == 0);
    ASSERT_TRUE("empty roots has no parse error", err[0] == '\0');
    bookmarks_mode_free(&mode);
}

static void test_url_filter(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    char err[256] = {0};
    int n = bookmarks_parse_chrome(&mode, kSchemeJson, "Default", "X",
                                   err, sizeof(err));
    ASSERT_TRUE("only 4 allowed schemes survive", n == 4);

    ASSERT_TRUE("http accepted", bookmarks_url_is_allowed("http://x"));
    ASSERT_TRUE("https accepted", bookmarks_url_is_allowed("https://x"));
    ASSERT_TRUE("ftp accepted", bookmarks_url_is_allowed("ftp://x"));
    ASSERT_TRUE("file accepted", bookmarks_url_is_allowed("file:///x"));
    ASSERT_TRUE("javascript rejected", !bookmarks_url_is_allowed("javascript:1"));
    ASSERT_TRUE("chrome rejected", !bookmarks_url_is_allowed("chrome://flags"));
    ASSERT_TRUE("chrome-extension rejected",
                !bookmarks_url_is_allowed("chrome-extension://x/y"));
    ASSERT_TRUE("data rejected", !bookmarks_url_is_allowed("data:text/plain,a"));
    ASSERT_TRUE("empty rejected", !bookmarks_url_is_allowed(""));
    ASSERT_TRUE("null rejected", !bookmarks_url_is_allowed(NULL));
    bookmarks_mode_free(&mode);
}

static void test_format_match_text_includes_bm_marker(void) {
    BookmarkEntry e;
    memset(&e, 0, sizeof(e));
    g_strlcpy(e.profile_label, "Default", sizeof(e.profile_label));
    g_strlcpy(e.name, "Some Page", sizeof(e.name));
    g_strlcpy(e.folder_breadcrumb, "Docs > Deep", sizeof(e.folder_breadcrumb));
    g_strlcpy(e.url, "https://example.test/", sizeof(e.url));
    char buf[1024];
    bookmarks_format_match_text(&e, buf, sizeof(buf));
    ASSERT_TRUE("match string starts with [bm]", g_str_has_prefix(buf, "[bm]"));
    ASSERT_TRUE("match string includes full breadcrumb",
                strstr(buf, "Docs > Deep") != NULL);
}

static void test_folder_truncation(void) {
    char out[64];
    bookmarks_truncate_folder_display("ab", out, sizeof(out), 10);
    ASSERT_TRUE("short stays as-is", strcmp(out, "ab") == 0);

    /* "Docs > Deep" is 11 chars, > 10. Left-clip keeps last 9 with leading "…". */
    bookmarks_truncate_folder_display("Docs > Deep", out, sizeof(out), 10);
    ASSERT_TRUE("long left-clipped with ellipsis prefix",
                g_str_has_prefix(out, "…"));
    /* Visible part after ellipsis should end with the deepest folder name */
    ASSERT_TRUE("deepest folder preserved", g_str_has_suffix(out, "Deep"));
}

static void test_filter_empty_preserves_order(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    char err[256] = {0};
    bookmarks_parse_chrome(&mode, kSampleJson, "Default", "User", err, sizeof(err));
    bookmarks_filter(&mode, "");
    ASSERT_TRUE("empty filter includes all rows",
                (int)mode.filtered_indices->len == (int)mode.entries->len);
    ASSERT_TRUE("empty filter preserves insertion order",
                g_array_index(mode.filtered_indices, int, 0) == 0 &&
                g_array_index(mode.filtered_indices, int, 1) == 1);
    bookmarks_mode_free(&mode);
}

static void test_filter_bm_marker_surfaces_all(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    char err[256] = {0};
    bookmarks_parse_chrome(&mode, kSampleJson, "Default", "User", err, sizeof(err));
    bookmarks_filter(&mode, "bm");
    ASSERT_TRUE("bm token surfaces every row",
                (int)mode.filtered_indices->len == (int)mode.entries->len);
    bookmarks_mode_free(&mode);
}

static void test_filter_name_outranks_url(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    /* Row A: matches in URL. Row B: matches in name. Query should prefer B. */
    BookmarkEntry a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    g_strlcpy(a.profile_label, "Default", sizeof(a.profile_label));
    g_strlcpy(a.name, "Unrelated", sizeof(a.name));
    g_strlcpy(a.url, "https://target.example/", sizeof(a.url));
    g_strlcpy(b.profile_label, "Default", sizeof(b.profile_label));
    g_strlcpy(b.name, "target page", sizeof(b.name));
    g_strlcpy(b.url, "https://other.example/", sizeof(b.url));
    g_array_append_val(mode.entries, a);
    g_array_append_val(mode.entries, b);

    bookmarks_filter(&mode, "target");
    ASSERT_TRUE("two hits", mode.filtered_indices->len == 2);
    ASSERT_TRUE("name-hit row ranked first",
                g_array_index(mode.filtered_indices, int, 0) == 1);
    bookmarks_mode_free(&mode);
}

static void test_cap_enforcement(void) {
    BookmarksMode mode;
    bookmarks_mode_init(&mode);
    /* Pre-seed entries close to the cap, then feed JSON with extras and
     * confirm appends stop at BOOKMARKS_MAX_ROWS. */
    BookmarkEntry filler;
    memset(&filler, 0, sizeof(filler));
    g_strlcpy(filler.name, "filler", sizeof(filler.name));
    g_strlcpy(filler.url, "https://filler/", sizeof(filler.url));
    for (int i = 0; i < BOOKMARKS_MAX_ROWS - 1; i++) {
        g_array_append_val(mode.entries, filler);
    }
    const char *extras_json =
        "{\"roots\":{\"bookmark_bar\":{\"type\":\"folder\",\"children\":["
        "{\"type\":\"url\",\"name\":\"a\",\"url\":\"https://a/\"},"
        "{\"type\":\"url\",\"name\":\"b\",\"url\":\"https://b/\"},"
        "{\"type\":\"url\",\"name\":\"c\",\"url\":\"https://c/\"}]},"
        "\"other\":{\"type\":\"folder\",\"children\":[]},"
        "\"synced\":{\"type\":\"folder\",\"children\":[]}}}";
    char err[256] = {0};
    bookmarks_parse_chrome(&mode, extras_json, "Default", "U", err, sizeof(err));
    ASSERT_TRUE("cap holds at BOOKMARKS_MAX_ROWS",
                (int)mode.entries->len == BOOKMARKS_MAX_ROWS);
    bookmarks_mode_free(&mode);
}

int main(void) {
    test_parser_basic();
    test_parser_name_fallback_to_url();
    test_parser_profile_label_fallback();
    test_parser_malformed();
    test_parser_empty_roots();
    test_url_filter();
    test_format_match_text_includes_bm_marker();
    test_folder_truncation();
    test_filter_empty_preserves_order();
    test_filter_bm_marker_surfaces_all();
    test_filter_name_outranks_url();
    test_cap_enforcement();

    printf("\nResults: %d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
