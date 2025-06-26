#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include "test_utils.h"
#include "../src/filter.h"
#include "../src/history.h"
#include "../src/constants.h"

// Mock the history functions since filter.c calls them
void update_history(AppData *app) {
    // For testing, just keep the windows as-is
}

void partition_and_reorder(AppData *app) {
    // For testing, just keep the windows as-is
}

// Test word boundary matching
void test_word_boundary_matching() {
    printf("\n=== Testing Word Boundary Matching ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows
    add_test_window(&app, create_test_window(1, "Visual Studio Code", "Code", "code", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Firefox Developer Edition", "Firefox", "firefox", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Commodoro Timer", "Commodoro", "commodoro", "Normal", 0));
    add_test_window(&app, create_test_window(4, "Google Chrome", "Chrome", "chrome", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test "vis" matches "Visual Studio Code"
    filter_windows(&app, "vis");
    ASSERT_EQ(app.filtered_count, 1, "Filter 'vis' should match 1 window");
    ASSERT_STR_EQ(app.filtered[0].title, "Visual Studio Code", "Should match 'Visual Studio Code'");
    
    // Test "comm" matches "Commodoro Timer"
    filter_windows(&app, "comm");
    ASSERT_EQ(app.filtered_count, 1, "Filter 'comm' should match 1 window");
    ASSERT_STR_EQ(app.filtered[0].title, "Commodoro Timer", "Should match 'Commodoro Timer'");
    
    // Test "fire" matches "Firefox Developer Edition"
    filter_windows(&app, "fire");
    ASSERT_EQ(app.filtered_count, 1, "Filter 'fire' should match 1 window");
    ASSERT_STR_EQ(app.filtered[0].title, "Firefox Developer Edition", "Should match 'Firefox Developer Edition'");
}

// Test initials matching
void test_initials_matching() {
    printf("\n=== Testing Initials Matching ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows
    add_test_window(&app, create_test_window(1, "Visual Studio Code", "Code", "code", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Daniel Dario Lukic", "Terminal", "gnome-terminal", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Volume Control", "Pavucontrol", "pavucontrol", "Normal", 0));
    add_test_window(&app, create_test_window(4, "Firefox Developer", "Firefox", "firefox", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test "vsc" matches "Visual Studio Code"
    filter_windows(&app, "vsc");
    ASSERT_EQ(app.filtered_count, 1, "Filter 'vsc' should match 1 window");
    ASSERT_STR_EQ(app.filtered[0].title, "Visual Studio Code", "Should match 'Visual Studio Code' by initials");
    
    // Test "ddl" matches "Daniel Dario Lukic"
    filter_windows(&app, "ddl");
    ASSERT_EQ(app.filtered_count, 1, "Filter 'ddl' should match 1 window");
    ASSERT_STR_EQ(app.filtered[0].title, "Daniel Dario Lukic", "Should match 'Daniel Dario Lukic' by initials");
    
    // Test "vc" matches both "Visual Studio Code" and "Volume Control"
    filter_windows(&app, "vc");
    ASSERT(app.filtered_count >= 1, "Filter 'vc' should match at least 1 window");
    // Both are valid matches, so just verify we got matches
}

// Test subsequence matching
void test_subsequence_matching() {
    printf("\n=== Testing Subsequence Matching ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows
    add_test_window(&app, create_test_window(1, "Thunderbird Mail", "Thunderbird", "thunderbird", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Terminal Emulator", "Terminal", "xterm", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Text Editor", "Gedit", "gedit", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test "thd" matches "Thunderbird" as subsequence
    filter_windows(&app, "thd");
    ASSERT_EQ(app.filtered_count, 1, "Filter 'thd' should match 1 window");
    ASSERT_STR_EQ(app.filtered[0].title, "Thunderbird Mail", "Should match 'Thunderbird' by subsequence");
    
    // Test "term" matches windows with "term" subsequence
    filter_windows(&app, "term");
    ASSERT(app.filtered_count >= 1, "Filter 'term' should match at least 1 window");
    // Word boundary match would take priority
    ASSERT_STR_EQ(app.filtered[0].title, "Terminal Emulator", "Should match 'Terminal Emulator' first");
}

// Test scoring and ordering
void test_scoring_order() {
    printf("\n=== Testing Scoring and Order ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows that will match "code" in different ways
    add_test_window(&app, create_test_window(1, "VS Code Editor", "Code", "code", "Normal", 0));  // Fuzzy match
    add_test_window(&app, create_test_window(2, "Code Review Tool", "Review", "review", "Normal", 0));  // Word boundary
    add_test_window(&app, create_test_window(3, "Unicode Display", "Display", "display", "Normal", 0));  // Subsequence
    add_test_window(&app, create_test_window(4, "Encode Utility", "Encode", "encode", "Normal", 0));  // Fuzzy
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test "code" - should prioritize word boundary match
    filter_windows(&app, "code");
    ASSERT(app.filtered_count >= 2, "Filter 'code' should match at least 2 windows");
    ASSERT_STR_EQ(app.filtered[0].title, "Code Review Tool", "Word boundary match should be first");
}

// Test case-insensitive matching
void test_case_insensitive() {
    printf("\n=== Testing Case Insensitive Matching ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows
    add_test_window(&app, create_test_window(1, "FIREFOX Browser", "Firefox", "firefox", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Visual Studio CODE", "Code", "code", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test lowercase filter matches uppercase title
    filter_windows(&app, "firefox");
    ASSERT_EQ(app.filtered_count, 1, "Lowercase 'firefox' should match uppercase title");
    ASSERT_STR_EQ(app.filtered[0].title, "FIREFOX Browser", "Should match 'FIREFOX Browser'");
    
    // Test uppercase filter matches mixed case
    filter_windows(&app, "CODE");
    ASSERT_EQ(app.filtered_count, 1, "Uppercase 'CODE' should match");
    ASSERT_STR_EQ(app.filtered[0].title, "Visual Studio CODE", "Should match 'Visual Studio CODE'");
}

// Test empty filter behavior
void test_empty_filter() {
    printf("\n=== Testing Empty Filter ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows
    add_test_window(&app, create_test_window(1, "Window 1", "Class1", "inst1", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Window 2", "Class2", "inst2", "Normal", 1));
    add_test_window(&app, create_test_window(3, "Window 3", "Class3", "inst3", "Special", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test empty filter shows all windows
    filter_windows(&app, "");
    ASSERT_EQ(app.filtered_count, 3, "Empty filter should show all windows");
    ASSERT_EQ(app.selected_index, 0, "Should select first window");
}

// Test filtering on class and instance names
void test_class_instance_filtering() {
    printf("\n=== Testing Class and Instance Filtering ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows
    add_test_window(&app, create_test_window(1, "Browser Window", "Firefox", "firefox", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Terminal", "Gnome-terminal", "gnome-terminal-server", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Editor", "Code", "code", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test filtering by class name
    filter_windows(&app, "firefox");
    ASSERT_EQ(app.filtered_count, 1, "Should match by class name");
    ASSERT_STR_EQ(app.filtered[0].class_name, "Firefox", "Should match Firefox class");
    
    // Test filtering by instance name
    filter_windows(&app, "terminal-server");
    ASSERT_EQ(app.filtered_count, 1, "Should match by instance name");
    ASSERT_STR_EQ(app.filtered[0].instance, "gnome-terminal-server", "Should match terminal instance");
}

// Test Alt-Tab swap behavior
void test_alttab_swap() {
    printf("\n=== Testing Alt-Tab Swap Behavior ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows in specific order
    add_test_window(&app, create_test_window(1, "Current Window", "Current", "current", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Previous Window", "Previous", "previous", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Third Window", "Third", "third", "Normal", 0));
    
    // Set up history with specific order
    add_history_window(&app, app.windows[0]);  // Most recent
    add_history_window(&app, app.windows[1]);  // Previous
    add_history_window(&app, app.windows[2]);  // Third
    
    // Test empty filter - should swap first two
    filter_windows(&app, "");
    ASSERT_EQ(app.filtered_count, 3, "Should have all windows");
    ASSERT_STR_EQ(app.filtered[0].title, "Previous Window", "First displayed should be previous (swapped)");
    ASSERT_STR_EQ(app.filtered[1].title, "Current Window", "Second displayed should be current (swapped)");
    ASSERT_STR_EQ(app.filtered[2].title, "Third Window", "Third should remain in place");
    ASSERT_EQ(app.selected_index, 0, "Should select first (previous) window for Alt-Tab behavior");
}

int main() {
    printf("=== Running Filter Unit Tests ===\n");
    
    test_word_boundary_matching();
    test_initials_matching();
    test_subsequence_matching();
    test_scoring_order();
    test_case_insensitive();
    test_empty_filter();
    test_class_instance_filtering();
    test_alttab_swap();
    
    PRINT_TEST_SUMMARY();
    
    return (tests_run - tests_passed) > 0 ? 1 : 0;
}