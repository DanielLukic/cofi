#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/window_matcher.h"

// Test helper to create a WindowInfo
WindowInfo create_test_window(Window id, const char *title, const char *class_name, 
                              const char *instance, const char *type) {
    WindowInfo window;
    window.id = id;
    strncpy(window.title, title, MAX_TITLE_LEN - 1);
    window.title[MAX_TITLE_LEN - 1] = '\0';
    strncpy(window.class_name, class_name, MAX_CLASS_LEN - 1);
    window.class_name[MAX_CLASS_LEN - 1] = '\0';
    strncpy(window.instance, instance, MAX_CLASS_LEN - 1);
    window.instance[MAX_CLASS_LEN - 1] = '\0';
    strncpy(window.type, type, 15);
    window.type[15] = '\0';
    window.desktop = 0;
    window.pid = 0;
    return window;
}

void test_windows_match_exact() {
    printf("Testing windows_match_exact...\n");
    
    WindowInfo w1 = create_test_window(1, "Firefox", "Firefox", "firefox", "Normal");
    WindowInfo w2 = create_test_window(2, "Firefox", "Firefox", "firefox", "Normal");
    WindowInfo w3 = create_test_window(3, "Firefox - Page 1", "Firefox", "firefox", "Normal");
    WindowInfo w4 = create_test_window(4, "Firefox", "Chrome", "firefox", "Normal");
    
    // Exact match
    assert(windows_match_exact(&w1, &w2) == true);
    
    // Different title
    assert(windows_match_exact(&w1, &w3) == false);
    
    // Different class
    assert(windows_match_exact(&w1, &w4) == false);
    
    // NULL checks
    assert(windows_match_exact(NULL, &w1) == false);
    assert(windows_match_exact(&w1, NULL) == false);
    
    printf("  ✓ All tests passed\n");
}

void test_get_title_base_length() {
    printf("Testing get_title_base_length...\n");
    
    assert(get_title_base_length("Firefox - Page 1") == 8); // "Firefox "
    assert(get_title_base_length("VS Code - file.c") == 8); // "VS Code "
    assert(get_title_base_length("No dash here") == 0);
    assert(get_title_base_length("") == 0);
    assert(get_title_base_length(NULL) == 0);
    assert(get_title_base_length("Multiple - dashes - here") == 9); // "Multiple "
    
    printf("  ✓ All tests passed\n");
}

void test_titles_match_fuzzy() {
    printf("Testing titles_match_fuzzy...\n");
    
    // Exact match
    assert(titles_match_fuzzy("Firefox", "Firefox") == true);
    
    // Dash-based matching
    assert(titles_match_fuzzy("Firefox - Page 1", "Firefox - Page 2") == true);
    assert(titles_match_fuzzy("VS Code - file1.c", "VS Code - file2.c") == true);
    assert(titles_match_fuzzy("Firefox - Page 1", "Chrome - Page 1") == false);
    
    // Containment matching
    assert(titles_match_fuzzy("Commodoro", "Commodoro Timer") == true);
    assert(titles_match_fuzzy("Timer - Commodoro", "Commodoro") == true);
    
    // No match
    assert(titles_match_fuzzy("Firefox", "Chrome") == false);
    assert(titles_match_fuzzy("Firefox - Page 1", "Chrome - Page 2") == false);
    
    // NULL checks
    assert(titles_match_fuzzy(NULL, "Firefox") == false);
    assert(titles_match_fuzzy("Firefox", NULL) == false);
    assert(titles_match_fuzzy(NULL, NULL) == false);
    
    printf("  ✓ All tests passed\n");
}

void test_windows_match_fuzzy() {
    printf("Testing windows_match_fuzzy...\n");
    
    WindowInfo w1 = create_test_window(1, "Firefox - Page 1", "Firefox", "firefox", "Normal");
    WindowInfo w2 = create_test_window(2, "Firefox - Page 2", "Firefox", "firefox", "Normal");
    WindowInfo w3 = create_test_window(3, "Firefox", "Firefox", "firefox", "Normal");
    WindowInfo w4 = create_test_window(4, "Firefox - Page 1", "Firefox", "chrome", "Normal");
    WindowInfo w5 = create_test_window(5, "Firefox - Page 1", "Firefox", "firefox", "Special");
    
    // Fuzzy match with same class/instance/type
    assert(windows_match_fuzzy(&w1, &w2) == true);
    assert(windows_match_fuzzy(&w1, &w3) == true);
    
    // Different instance
    assert(windows_match_fuzzy(&w1, &w4) == false);
    
    // Different type
    assert(windows_match_fuzzy(&w1, &w5) == false);
    
    // NULL checks
    assert(windows_match_fuzzy(NULL, &w1) == false);
    assert(windows_match_fuzzy(&w1, NULL) == false);
    
    printf("  ✓ All tests passed\n");
}

void test_commodoro_case() {
    printf("Testing Commodoro specific case...\n");
    
    WindowInfo stored = create_test_window(0x640000c, "Commodoro", "Commodoro", "commodoro", "Normal");
    WindowInfo current = create_test_window(0x3e0000c, "Commodoro", "Commodoro", "commodoro", "Normal");
    
    // Should match exactly
    assert(windows_match_exact(&stored, &current) == true);
    
    // Even with different IDs, the content matches
    stored.id = 1;
    current.id = 2;
    assert(windows_match_exact(&stored, &current) == true);
    
    printf("  ✓ All tests passed\n");
}

int main() {
    printf("Running window matcher tests...\n\n");
    
    test_windows_match_exact();
    test_get_title_base_length();
    test_titles_match_fuzzy();
    test_windows_match_fuzzy();
    test_commodoro_case();
    
    printf("\nAll tests passed! ✓\n");
    return 0;
}