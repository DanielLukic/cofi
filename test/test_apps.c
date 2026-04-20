/*
 * Behavioral tests for the Apps tab (TFD-467).
 *
 * Tests pure logic only: filter_entries and sort_entries.
 * GIO-dependent functions (apps_load, apps_launch) are not called.
 *
 * UX contract:
 *   - empty query returns all entries
 *   - query matches against name, generic_name, and keywords
 *   - non-matching query returns empty
 *   - entries are sorted alphabetically by name (ascending)
 *   - NoDisplay / hidden entries are excluded at load time (not tested here —
 *     apps_load() is GIO-dependent; exclusion logic is in populate_entry guard)
 */

#include <stdio.h>
#include <string.h>

/* Pull in match.h for has_match (linked separately). */
#include "../src/match.h"

/* Stub log_* so apps.c compiles cleanly without the log subsystem. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#define log_trace(...)  do {} while (0)
#define log_debug(...)  do {} while (0)
#define log_info(...)   do {} while (0)
#define log_warn(...)   do {} while (0)
#define log_error(...)  do {} while (0)
#pragma GCC diagnostic pop

/*
 * Provide a minimal GLib stub so apps.c can be included without a live GIO
 * environment.  Only g_utf8_collate is actually called by the pure functions
 * under test (apps_sort_entries uses qsort with app_entry_cmp).
 * The real GLib is linked via $(LDFLAGS), so we don't need to stub it —
 * this comment documents the assumption.
 */
#include <gio/gdesktopappinfo.h>

/* Include the module under test directly (pure functions only). */
#include "../src/apps.c"

/* ---- Test harness ---- */

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else       { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

#define ASSERT_EQ_INT(name, a, b) \
    ASSERT_TRUE(name, (a) == (b))

#define ASSERT_STR_EQ(name, a, b) \
    ASSERT_TRUE(name, strcmp((a), (b)) == 0)

/* ---- Helpers ---- */

static AppEntry make_entry(const char *name,
                           const char *generic_name,
                           const char *keywords) {
    AppEntry e;
    memset(&e, 0, sizeof(e));
    strncpy(e.name, name, sizeof(e.name) - 1);
    strncpy(e.generic_name, generic_name, sizeof(e.generic_name) - 1);
    strncpy(e.keywords, keywords, sizeof(e.keywords) - 1);
    e.info = NULL;  /* not used in pure functions */
    return e;
}

/* ---- Filter tests ---- */

static void test_empty_query_returns_all(void) {
    AppEntry src[3] = {
        make_entry("Firefox", "Web Browser", "web browser internet"),
        make_entry("Terminal", "Terminal Emulator", "shell console"),
        make_entry("Gimp", "Image Editor", "graphics photo"),
    };
    AppEntry out[3];
    int count = 0;

    apps_filter_entries("", src, 3, out, &count);

    ASSERT_EQ_INT("empty query: all returned", 3, count);
}

static void test_null_query_returns_all(void) {
    AppEntry src[2] = {
        make_entry("Firefox", "Web Browser", "web"),
        make_entry("Terminal", "Terminal Emulator", "shell"),
    };
    AppEntry out[2];
    int count = 0;

    apps_filter_entries(NULL, src, 2, out, &count);

    ASSERT_EQ_INT("null query: all returned", 2, count);
}

static void test_match_on_name(void) {
    AppEntry src[3] = {
        make_entry("Firefox", "Web Browser", "web"),
        make_entry("Terminal", "Terminal Emulator", "shell"),
        make_entry("Gimp", "Image Editor", "graphics"),
    };
    AppEntry out[3];
    int count = 0;

    apps_filter_entries("fire", src, 3, out, &count);

    ASSERT_EQ_INT("name match: one result", 1, count);
    ASSERT_STR_EQ("name match: correct entry", "Firefox", out[0].name);
}

static void test_match_on_generic_name(void) {
    AppEntry src[3] = {
        make_entry("Firefox", "Web Browser", "web"),
        make_entry("Chromium", "Web Browser", "web chrome"),
        make_entry("Terminal", "Terminal Emulator", "shell"),
    };
    AppEntry out[3];
    int count = 0;

    apps_filter_entries("emulator", src, 3, out, &count);

    ASSERT_EQ_INT("generic_name match: one result", 1, count);
    ASSERT_STR_EQ("generic_name match: correct entry", "Terminal", out[0].name);
}

static void test_match_on_keywords(void) {
    AppEntry src[3] = {
        make_entry("Firefox", "Web Browser", "web internet browser"),
        make_entry("Terminal", "Terminal Emulator", "shell console tty"),
        make_entry("Gimp", "Image Editor", "graphics photo raster"),
    };
    AppEntry out[3];
    int count = 0;

    apps_filter_entries("raster", src, 3, out, &count);

    ASSERT_EQ_INT("keyword match: one result", 1, count);
    ASSERT_STR_EQ("keyword match: correct entry", "Gimp", out[0].name);
}

static void test_no_match_returns_empty(void) {
    AppEntry src[2] = {
        make_entry("Firefox", "Web Browser", "web"),
        make_entry("Terminal", "Terminal Emulator", "shell"),
    };
    AppEntry out[2];
    int count = 0;

    apps_filter_entries("zzznomatch", src, 2, out, &count);

    ASSERT_EQ_INT("no match: zero results", 0, count);
}

static void test_multiple_matches(void) {
    AppEntry src[4] = {
        make_entry("Firefox", "Web Browser", "web"),
        make_entry("Chromium", "Web Browser", "web chrome"),
        make_entry("Terminal", "Terminal Emulator", "shell"),
        make_entry("Epiphany", "Web Browser", "web gnome"),
    };
    AppEntry out[4];
    int count = 0;

    apps_filter_entries("Web", src, 4, out, &count);

    ASSERT_EQ_INT("multi match: three results", 3, count);
}

static void test_empty_source_list(void) {
    AppEntry out[4];
    int count = 0;

    apps_filter_entries("firefox", NULL, 0, out, &count);

    ASSERT_EQ_INT("empty source: zero results", 0, count);
}

/* ---- Sort tests ---- */

static void test_sort_alphabetical(void) {
    AppEntry entries[4] = {
        make_entry("Zsh", "", ""),
        make_entry("Alacritty", "", ""),
        make_entry("Firefox", "", ""),
        make_entry("Bash", "", ""),
    };

    apps_sort_entries(entries, 4);

    ASSERT_STR_EQ("sort: first is Alacritty", "Alacritty", entries[0].name);
    ASSERT_STR_EQ("sort: second is Bash",     "Bash",      entries[1].name);
    ASSERT_STR_EQ("sort: third is Firefox",   "Firefox",   entries[2].name);
    ASSERT_STR_EQ("sort: fourth is Zsh",      "Zsh",       entries[3].name);
}

static void test_sort_single_entry(void) {
    AppEntry entries[1] = { make_entry("Solo", "", "") };
    apps_sort_entries(entries, 1);
    ASSERT_STR_EQ("sort single: unchanged", "Solo", entries[0].name);
}

static void test_sort_already_sorted(void) {
    AppEntry entries[3] = {
        make_entry("Alpha", "", ""),
        make_entry("Beta", "", ""),
        make_entry("Gamma", "", ""),
    };
    apps_sort_entries(entries, 3);
    ASSERT_STR_EQ("sort already sorted: first",  "Alpha", entries[0].name);
    ASSERT_STR_EQ("sort already sorted: second", "Beta",  entries[1].name);
    ASSERT_STR_EQ("sort already sorted: third",  "Gamma", entries[2].name);
}

/* ---- Cross-keyword regression: "thu" must not match Audacity (TFD-467) ---- */

/*
 * "thu" matches Audacity's concatenated keyword string via cross-word
 * subsequence: edi**t**ing -> c**h**annel -> freq**u**ency.
 * Keywords must be matched individually (per token), not as one long string.
 */
static void test_thu_selects_thunderbird_not_audacity(void) {
    AppEntry src[4] = {
        /* Audacity: semicolon-joined keywords produce cross-token "thu" match */
        make_entry("Audacity", "Sound Editor",
                   "sound;music editing;voice channel;frequency;modulation;audio trim;clipping;noise reduction;multi track audio editor;edit;mixing;WAV;AIFF;FLAC;MP2;MP3;"),
        /* Control Center: generic name produces cross-token "thu" via
         * "The MATE configuration tool". */
        make_entry("Control Center", "The MATE configuration tool",
                   "MATE;control;center;configuration;tool;desktop;preferences;"),
        make_entry("Thunderbird Mail", "Mail Client",
                   "Email;E-mail;Newsgroup;Feed;RSS;"),
        make_entry("Atril Document Viewer", "Document Viewer",
                   "MATE;document;viewer;pdf;dvi;ps;xps;tiff;pixbuf;djvu;comics;"),
    };
    AppEntry out[4];
    int count = 0;

    apps_filter_entries("thu", src, 4, out, &count);

    ASSERT_EQ_INT("thu: only Thunderbird matches", 1, count);
    ASSERT_STR_EQ("thu: first is Thunderbird Mail", "Thunderbird Mail", out[0].name);
}

static void test_audac_prefers_audacious_and_audacity_only(void) {
    AppEntry src[4] = {
        make_entry("Atril Document Viewer", "Document Viewer",
                   "MATE;document;viewer;pdf;dvi;ps;xps;tiff;pixbuf;djvu;comics;"),
        make_entry("Audacious", "Music Player",
                   "audio;player;audacious;music;gtk;"),
        make_entry("Audacity", "Sound Editor",
                   "sound;music editing;voice channel;frequency;modulation;audio trim;clipping;noise reduction;multi track audio editor;edit;mixing;WAV;AIFF;FLAC;MP2;MP3;"),
        make_entry("PulseAudio Volume Control", "Volume Control",
                   "pavucontrol;Microphone;Volume;Fade;Balance;Headset;Speakers;Headphones;Audio;Mixer;Output;Input;Devices;Playback;Recording;System Sounds;Sound Card;Settings;Preferences;"),
    };
    AppEntry out[4];
    int count = 0;

    apps_filter_entries("audac", src, 4, out, &count);

    ASSERT_EQ_INT("audac: only Audacious and Audacity match", 2, count);
    ASSERT_STR_EQ("audac: first is Audacious", "Audacious", out[0].name);
    ASSERT_STR_EQ("audac: second is Audacity", "Audacity", out[1].name);
}

/* ---- Cross-field match regression (TFD-467) ---- */

/*
 * "audac" should NOT match "Atril Document Viewer" even though a subsequence
 * a-u-d-a-c exists across the concatenated name+generic_name+keywords string
 * ("Atril"->"document"->"Document"->"MATE"->"document").
 * Each field must be matched independently; cross-field subsequences are false
 * positives and cause misleading results.
 */
static void test_cross_field_match_rejected(void) {
    AppEntry src[3] = {
        /* Atril: cross-field fzy match of "audac" via name+generic+keywords */
        make_entry("Atril Document Viewer", "Document Viewer",
                   "MATE document viewer pdf dvi ps xps tiff pixbuf djvu comics"),
        make_entry("Audacious", "Music Player",
                   "audio player audacious music gtk"),
        make_entry("Audacity", "Sound Editor",
                   "sound music editing voice"),
    };
    AppEntry out[3];
    int count = 0;

    apps_filter_entries("audac", src, 3, out, &count);

    ASSERT_EQ_INT("cross-field: only Audacious+Audacity match", 2, count);
    ASSERT_STR_EQ("cross-field: first is Audacious", "Audacious", out[0].name);
    ASSERT_STR_EQ("cross-field: second is Audacity", "Audacity",  out[1].name);
}

/* ---- Filter preserves order (post-sort input stays sorted in output) ---- */

static void test_filter_preserves_relative_order(void) {
    AppEntry src[4] = {
        make_entry("Alacritty", "Terminal Emulator", "shell"),
        make_entry("Firefox", "Web Browser", "web"),
        make_entry("Foot", "Terminal Emulator", "shell wayland"),
        make_entry("Zsh", "Shell", "shell terminal"),
    };
    AppEntry out[4];
    int count = 0;

    /* "terminal" matches generic_name of Alacritty and Foot */
    apps_filter_entries("Terminal", src, 4, out, &count);

    ASSERT_EQ_INT("order: two matches", 2, count);
    ASSERT_STR_EQ("order: Alacritty first", "Alacritty", out[0].name);
    ASSERT_STR_EQ("order: Foot second",     "Foot",      out[1].name);
}

/* ---- Main ---- */

int main(void) {
    /* Filter tests */
    test_empty_query_returns_all();
    test_null_query_returns_all();
    test_match_on_name();
    test_match_on_generic_name();
    test_match_on_keywords();
    test_no_match_returns_empty();
    test_multiple_matches();
    test_empty_source_list();

    /* Sort tests */
    test_sort_alphabetical();
    test_sort_single_entry();
    test_sort_already_sorted();

    /* Combined */
    test_filter_preserves_relative_order();

    /* Regression */
    test_thu_selects_thunderbird_not_audacity();
    test_audac_prefers_audacious_and_audacity_only();
    test_cross_field_match_rejected();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return (fail == 0) ? 0 : 1;
}
