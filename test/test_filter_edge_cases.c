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

// Test consecutive word matching edge case
void test_consecutive_word_matching() {
    printf("\n=== Testing Consecutive Word Matching ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows with similar patterns
    add_test_window(&app, create_test_window(1, "Code Fix - Bug #123", "Terminal", "terminal", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Config", "Editor", "editor", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Coding | Focus Mode", "IDE", "ide", "Normal", 0));
    add_test_window(&app, create_test_window(4, "Certificate File", "Browser", "browser", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test "cf" - currently matches via initials, not word boundary
    filter_windows(&app, "cf");
    ASSERT(app.filtered_count >= 2, "Filter 'cf' should match at least 2 windows");
    
    // Print actual results for debugging
    printf("Filtered results for 'cf':\n");
    for (int i = 0; i < app.filtered_count && i < 3; i++) {
        printf("  [%d] %s\n", i, app.filtered[i].title);
    }
    
    // Note: "cf" doesn't match as word boundary in any title
    // It matches via initials: Code Fix, Coding Focus, Certificate File
    // They all get the same initials score
    printf("Note: 'cf' matches via initials, not word boundaries\n");
    
    // Now test a real word boundary case where we want enhanced scoring
    printf("\nTesting word boundary matches:\n");
    
    // Clear windows
    app.window_count = 0;
    app.history_count = 0;
    app.filtered_count = 0;
    
    // Add windows where "code" appears as a word boundary
    add_test_window(&app, create_test_window(1, "Code Review", "Terminal", "terminal", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Unicode", "Editor", "editor", "Normal", 0));
    add_test_window(&app, create_test_window(3, "VS Code", "IDE", "ide", "Normal", 0));
    add_test_window(&app, create_test_window(4, "Codebase", "Browser", "browser", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test "code" - should prioritize "Code Review" (at start) over "VS Code"
    filter_windows(&app, "code");
    ASSERT(app.filtered_count >= 2, "Filter 'code' should match at least 2 windows");
    
    printf("Filtered results for 'code':\n");
    for (int i = 0; i < app.filtered_count && i < 4; i++) {
        printf("  [%d] %s\n", i, app.filtered[i].title);
    }
    
    // "Code Review" and "Codebase" should score higher (match at start of title)
    // than "VS Code" (match not at start)
    ASSERT_STR_EQ(app.filtered[0].title, "Code Review", 
                  "Word boundary at start should rank first");
}

// Test special character word boundaries
void test_special_char_word_boundaries() {
    printf("\n=== Testing Special Character Word Boundaries ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add test windows with various special characters
    add_test_window(&app, create_test_window(1, "Firefox | Developer", "Firefox", "firefox", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Code-Review Tool", "Tool", "tool", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Project_Manager", "Manager", "manager", "Normal", 0));
    add_test_window(&app, create_test_window(4, "Build.System", "Build", "build", "Normal", 0));
    add_test_window(&app, create_test_window(5, "Data(Processing)", "App", "app", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test various special characters as word boundaries
    filter_windows(&app, "dev");
    ASSERT(app.filtered_count >= 1, "Filter 'dev' should match Firefox | Developer");
    ASSERT_STR_EQ(app.filtered[0].title, "Firefox | Developer", "Pipe should act as word boundary");
    
    filter_windows(&app, "rev");
    ASSERT(app.filtered_count >= 1, "Filter 'rev' should match Code-Review");
    ASSERT_STR_EQ(app.filtered[0].title, "Code-Review Tool", "Hyphen should act as word boundary");
    
    filter_windows(&app, "man");
    ASSERT(app.filtered_count >= 1, "Filter 'man' should match Project_Manager");
    ASSERT_STR_EQ(app.filtered[0].title, "Project_Manager", "Underscore should act as word boundary");
    
    filter_windows(&app, "sys");
    ASSERT(app.filtered_count >= 1, "Filter 'sys' should match Build.System");
    ASSERT_STR_EQ(app.filtered[0].title, "Build.System", "Dot should act as word boundary");
    
    filter_windows(&app, "proc");
    ASSERT(app.filtered_count >= 1, "Filter 'proc' should match Data(Processing)");
    ASSERT_STR_EQ(app.filtered[0].title, "Data(Processing)", "Parenthesis should act as word boundary");
}

// Test scoring differentiation for multi-word matches
void test_multiword_scoring_priority() {
    printf("\n=== Testing Multi-word Scoring Priority ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add windows where filter matches different numbers of words
    add_test_window(&app, create_test_window(1, "Test Driven Development", "Editor", "editor", "Normal", 0));
    add_test_window(&app, create_test_window(2, "TDD", "Terminal", "terminal", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Todo", "App", "app", "Normal", 0));
    add_test_window(&app, create_test_window(4, "Technical Documentation Draft", "Browser", "browser", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test "tdd" - matches initials of 3 words vs 1 word
    filter_windows(&app, "tdd");
    ASSERT(app.filtered_count >= 2, "Filter 'tdd' should match multiple windows");
    
    printf("Filtered results for 'tdd':\n");
    for (int i = 0; i < app.filtered_count && i < 4; i++) {
        printf("  [%d] %s\n", i, app.filtered[i].title);
    }
    
    // Document current behavior
    // TODO: After implementing enhanced scoring, multi-word matches should rank higher
    printf("Note: Currently initials matching doesn't consider word count\n");
}

// Test edge case with similar prefixes
void test_similar_prefix_differentiation() {
    printf("\n=== Testing Similar Prefix Differentiation ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add windows with similar starting patterns
    add_test_window(&app, create_test_window(1, "Config Editor", "Editor", "editor", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Configuration", "Settings", "settings", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Console Output", "Terminal", "terminal", "Normal", 0));
    add_test_window(&app, create_test_window(4, "Code Editor", "IDE", "ide", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test "con" - should match all starting with "con"
    filter_windows(&app, "con");
    ASSERT(app.filtered_count >= 2, "Filter 'con' should match multiple windows");
    
    // Test "conf" - should narrow down to config-related
    filter_windows(&app, "conf");
    ASSERT(app.filtered_count >= 2, "Filter 'conf' should match config windows");
    
    printf("Filtered results for 'conf':\n");
    for (int i = 0; i < app.filtered_count && i < 3; i++) {
        printf("  [%d] %s\n", i, app.filtered[i].title);
    }
}

// Test complex real-world scenarios
void test_realworld_window_titles() {
    printf("\n=== Testing Real-world Window Titles ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add realistic window titles
    add_test_window(&app, create_test_window(1, "README.md - Visual Studio Code", "Code", "code", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Mozilla Firefox - GitHub - anthropics/cofi", "Firefox", "firefox", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Terminal - ~/Projects/cofi", "Gnome-terminal", "gnome-terminal", "Normal", 0));
    add_test_window(&app, create_test_window(4, "[Slack] - General - Anthropic", "Slack", "slack", "Normal", 0));
    add_test_window(&app, create_test_window(5, "*scratch* - GNU Emacs", "Emacs", "emacs", "Normal", 0));
    
    // Copy to history
    for (int i = 0; i < app.window_count; i++) {
        add_history_window(&app, app.windows[i]);
    }
    
    // Test various patterns
    filter_windows(&app, "gh");
    ASSERT(app.filtered_count >= 1, "Filter 'gh' should match GitHub");
    
    filter_windows(&app, "term");
    ASSERT(app.filtered_count >= 1, "Filter 'term' should match Terminal");
    
    filter_windows(&app, "vsc");
    ASSERT(app.filtered_count >= 1, "Filter 'vsc' should match Visual Studio Code");
    
    // Test ambiguous patterns
    filter_windows(&app, "pro");
    printf("Filtered results for 'pro' (Projects):\n");
    for (int i = 0; i < app.filtered_count && i < 2; i++) {
        printf("  [%d] %s\n", i, app.filtered[i].title);
    }
}

int main() {
    printf("=== Running Filter Edge Case Tests ===\n");
    
    test_consecutive_word_matching();
    test_special_char_word_boundaries();
    test_multiword_scoring_priority();
    test_similar_prefix_differentiation();
    test_realworld_window_titles();
    
    PRINT_TEST_SUMMARY();
    
    return (tests_run - tests_passed) > 0 ? 1 : 0;
}