/*
 * Behavioral tests for the Sinks tab parser.
 *
 * Tests pure pactl output parsing only. No subprocesses are spawned.
 */

#include <stdio.h>
#include <string.h>

#define COFI_SINKS_PARSER_TEST
#include "../src/sinks.c"

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

static void test_parse_long_sinks_preserves_pactl_order_and_marks_default(void) {
    const char *inventory =
        "Sink #41\n"
        "\tState: IDLE\n"
        "\tName: alsa_output.pci-0000_0a_00.4.analog-stereo\n"
        "\tDescription: Built-in Audio Analog Stereo\n"
        "\tDriver: PipeWire\n"
        "Sink #42\n"
        "\tState: IDLE\n"
        "\tName: alsa_output.pci-0000_0a_00.4.pro-output-7\n"
        "\tDescription: Built-in Audio Pro 7\n"
        "\tDriver: PipeWire\n"
        "Sink #55\n"
        "\tState: RUNNING\n"
        "\tName: bluez_output.11_22_33_44_55_66.1\n"
        "\tDescription: Headphones WH-1000XM5\n"
        "\tDriver: PipeWire\n";
    SinkEntry sinks[MAX_SINKS];
    char error[256];

    int count = sinks_parse_inventory_test_hook(
        inventory, "bluez_output.11_22_33_44_55_66.1",
        sinks, MAX_SINKS, error, sizeof(error));

    ASSERT_EQ_INT("parse count", 3, count);
    ASSERT_STR_EQ("first pactl sink preserved",
                  "alsa_output.pci-0000_0a_00.4.analog-stereo", sinks[0].name);
    ASSERT_TRUE("first sink unmarked", !sinks[0].is_default);
    ASSERT_STR_EQ("first description parsed",
                  "Built-in Audio Analog Stereo", sinks[0].description);
    ASSERT_STR_EQ("second pactl sink preserved",
                  "alsa_output.pci-0000_0a_00.4.pro-output-7", sinks[1].name);
    ASSERT_TRUE("second sink unmarked", !sinks[1].is_default);
    ASSERT_STR_EQ("default remains in pactl position",
                  "bluez_output.11_22_33_44_55_66.1", sinks[2].name);
    ASSERT_TRUE("default marked", sinks[2].is_default);
    ASSERT_STR_EQ("description parsed", "Headphones WH-1000XM5", sinks[2].description);
    ASSERT_STR_EQ("no parser error", "", error);
}

static void test_parse_pro_output_group_preserves_pactl_order(void) {
    const char *inventory =
        "Sink #10\n"
        "\tName: alsa_output.pci-0000_00_1f.3.pro-output-0\n"
        "\tDescription: Built-in Audio Pro\n"
        "Sink #11\n"
        "\tName: alsa_output.pci-0000_00_1f.3.pro-output-1\n"
        "\tDescription: Built-in Audio Pro 1\n"
        "Sink #13\n"
        "\tName: alsa_output.pci-0000_00_1f.3.pro-output-3\n"
        "\tDescription: Built-in Audio Pro 3\n"
        "Sink #17\n"
        "\tName: alsa_output.pci-0000_00_1f.3.pro-output-7\n"
        "\tDescription: Built-in Audio Pro 7\n"
        "Sink #18\n"
        "\tName: alsa_output.pci-0000_00_1f.3.pro-output-8\n"
        "\tDescription: Built-in Audio Pro 8\n"
        "Sink #19\n"
        "\tName: alsa_output.pci-0000_00_1f.3.pro-output-9\n"
        "\tDescription: Built-in Audio Pro 9\n"
        "Sink #20\n"
        "\tName: alsa_output.pci-0000_00_1f.3.pro-output-default\n"
        "\tDescription: Built-in Audio Default\n";
    SinkEntry sinks[MAX_SINKS];
    char error[256];

    int count = sinks_parse_inventory_test_hook(
        inventory, "alsa_output.pci-0000_00_1f.3.pro-output-default",
        sinks, MAX_SINKS, error, sizeof(error));

    ASSERT_EQ_INT("pro output count", 7, count);
    ASSERT_STR_EQ("pro output 0 first", "Built-in Audio Pro", sinks[0].description);
    ASSERT_STR_EQ("pro output 1 second", "Built-in Audio Pro 1", sinks[1].description);
    ASSERT_STR_EQ("pro output 3 third", "Built-in Audio Pro 3", sinks[2].description);
    ASSERT_STR_EQ("pro output 7 fourth", "Built-in Audio Pro 7", sinks[3].description);
    ASSERT_STR_EQ("pro output 8 fifth", "Built-in Audio Pro 8", sinks[4].description);
    ASSERT_STR_EQ("pro output 9 sixth", "Built-in Audio Pro 9", sinks[5].description);
    ASSERT_STR_EQ("default remains in pactl position", "Built-in Audio Default", sinks[6].description);
    ASSERT_TRUE("bottom is default", sinks[6].is_default);
}

static void test_parse_sink_name_with_spaces_and_unicode_description(void) {
    const char *inventory =
        "Sink #2\n"
        "\tState: SUSPENDED\n"
        "\tName: weird sink/name:hdmi output\n"
        "\tDescription: HDA NVidia Pro 7 – Wohnzimmer\n";
    SinkEntry sinks[MAX_SINKS];
    char error[256];

    int count = sinks_parse_inventory_test_hook(
        inventory, "weird sink/name:hdmi output",
        sinks, MAX_SINKS, error, sizeof(error));

    ASSERT_EQ_INT("weird name count", 1, count);
    ASSERT_STR_EQ("weird name preserved", "weird sink/name:hdmi output", sinks[0].name);
    ASSERT_TRUE("weird name can be default", sinks[0].is_default);
    ASSERT_STR_EQ("unicode description preserved",
                  "HDA NVidia Pro 7 – Wohnzimmer", sinks[0].description);
}

static void test_missing_description_falls_back_to_name(void) {
    const char *inventory =
        "Sink #9\n"
        "\tState: IDLE\n"
        "\tName: alsa_output.usb-DAC.analog-stereo\n";
    SinkEntry sinks[MAX_SINKS];
    char error[256];

    int count = sinks_parse_inventory_test_hook(
        inventory, "alsa_output.usb-DAC.analog-stereo",
        sinks, MAX_SINKS, error, sizeof(error));

    ASSERT_EQ_INT("missing description count", 1, count);
    ASSERT_STR_EQ("valid name preserved", "alsa_output.usb-DAC.analog-stereo", sinks[0].name);
    ASSERT_STR_EQ("description fallback", "alsa_output.usb-DAC.analog-stereo", sinks[0].description);
    ASSERT_STR_EQ("no fallback error", "", error);
}

static void test_empty_inventory_reports_no_sinks(void) {
    SinkEntry sinks[MAX_SINKS];
    char error[256];

    int count = sinks_parse_inventory_test_hook(
        "", "missing", sinks, MAX_SINKS, error, sizeof(error));

    ASSERT_EQ_INT("empty count", 0, count);
    ASSERT_STR_EQ("empty error", "No sinks found", error);
}

static void test_snapshot_tracks_order_and_default_only(void) {
    SinkEntry sinks[2];
    char first[1024];
    char same[1024];
    char changed[1024];

    memset(sinks, 0, sizeof(sinks));
    strcpy(sinks[0].name, "alsa_output.primary");
    strcpy(sinks[0].description, "Primary");
    sinks[0].is_default = TRUE;
    strcpy(sinks[1].name, "alsa_output.secondary");
    strcpy(sinks[1].description, "Secondary");
    sinks[1].is_default = FALSE;

    sinks_snapshot_test_hook(sinks, 2, first, sizeof(first));

    strcpy(sinks[0].description, "Primary Renamed");
    sinks_snapshot_test_hook(sinks, 2, same, sizeof(same));

    sinks[0].is_default = FALSE;
    sinks[1].is_default = TRUE;
    sinks_snapshot_test_hook(sinks, 2, changed, sizeof(changed));

    ASSERT_STR_EQ("snapshot ignores visual description", first, same);
    ASSERT_TRUE("snapshot detects default changes", strcmp(first, changed) != 0);
}

int main(void) {
    test_parse_long_sinks_preserves_pactl_order_and_marks_default();
    test_parse_pro_output_group_preserves_pactl_order();
    test_parse_sink_name_with_spaces_and_unicode_description();
    test_missing_description_falls_back_to_name();
    test_empty_inventory_reports_no_sinks();
    test_snapshot_tracks_order_and_default_only();

    printf("\nSinks parser tests: %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
