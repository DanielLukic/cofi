#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include "test_utils.h"
#include "../src/history.h"
#include "../src/x11_utils.h"

// Mock X11 functions
static int mock_active_window_id = 0;

int get_active_window_id(Display *display) {
    (void)display;  // Unused
    return mock_active_window_id;
}

// Test basic history update (window list synchronization)
void test_update_history_basic() {
    printf("\n=== Testing Basic History Update ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Add windows to current window list
    add_test_window(&app, create_test_window(1, "Firefox", "Firefox", "firefox", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Terminal", "Terminal", "terminal", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Editor", "Code", "code", "Normal", 0));
    
    // Update history
    update_history(&app);
    
    ASSERT_EQ(app.history_count, 3, "History should have 3 windows");
    ASSERT_EQ(app.history[0].id, 1, "First window should be Firefox");
    ASSERT_EQ(app.history[1].id, 2, "Second window should be Terminal");
    ASSERT_EQ(app.history[2].id, 3, "Third window should be Editor");
}

// Test window removal from history when closed
void test_window_removal() {
    printf("\n=== Testing Window Removal ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Start with 3 windows in history
    add_history_window(&app, create_test_window(1, "Firefox", "Firefox", "firefox", "Normal", 0));
    add_history_window(&app, create_test_window(2, "Terminal", "Terminal", "terminal", "Normal", 0));
    add_history_window(&app, create_test_window(3, "Editor", "Code", "code", "Normal", 0));
    
    // Current window list has only 2 windows (Terminal was closed)
    add_test_window(&app, create_test_window(1, "Firefox", "Firefox", "firefox", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Editor", "Code", "code", "Normal", 0));
    
    // Update history
    update_history(&app);
    
    ASSERT_EQ(app.history_count, 2, "History should have 2 windows after removal");
    ASSERT_EQ(app.history[0].id, 1, "Firefox should still be first");
    ASSERT_EQ(app.history[1].id, 3, "Editor should be second");
    
    // Verify Terminal was removed
    int terminal_found = 0;
    for (int i = 0; i < app.history_count; i++) {
        if (app.history[i].id == 2) terminal_found = 1;
    }
    ASSERT(!terminal_found, "Terminal should be removed from history");
}

// Test MRU ordering when active window changes
void test_mru_ordering() {
    printf("\n=== Testing MRU Ordering ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Set up initial history
    add_history_window(&app, create_test_window(1, "Firefox", "Firefox", "firefox", "Normal", 0));
    add_history_window(&app, create_test_window(2, "Terminal", "Terminal", "terminal", "Normal", 0));
    add_history_window(&app, create_test_window(3, "Editor", "Code", "code", "Normal", 0));
    
    // Copy to window list
    for (int i = 0; i < app.history_count; i++) {
        add_test_window(&app, app.history[i]);
    }
    
    // Initially, Firefox (id=1) is active
    app.active_window_id = 1;
    mock_active_window_id = 1;
    
    // Now Terminal (id=2) becomes active
    mock_active_window_id = 2;
    update_history(&app);
    
    ASSERT_EQ(app.history[0].id, 2, "Terminal should move to front");
    ASSERT_EQ(app.history[1].id, 1, "Firefox should move to second");
    ASSERT_EQ(app.history[2].id, 3, "Editor should stay third");
    
    // Now Editor (id=3) becomes active
    mock_active_window_id = 3;
    update_history(&app);
    
    ASSERT_EQ(app.history[0].id, 3, "Editor should move to front");
    ASSERT_EQ(app.history[1].id, 2, "Terminal should move to second");
    ASSERT_EQ(app.history[2].id, 1, "Firefox should move to third");
}

// Test cofi window exclusion
void test_cofi_exclusion() {
    printf("\n=== Testing Cofi Window Exclusion ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Set up history with cofi window in it
    add_history_window(&app, create_test_window(1, "Firefox", "Firefox", "firefox", "Normal", 0));
    add_history_window(&app, create_test_window(2, "cofi", "cofi", "cofi", "Normal", 0));
    add_history_window(&app, create_test_window(3, "Editor", "Code", "code", "Normal", 0));
    
    // Copy to window list
    for (int i = 0; i < app.history_count; i++) {
        add_test_window(&app, app.history[i]);
    }
    
    // Firefox is active initially
    app.active_window_id = 1;
    mock_active_window_id = 1;
    
    // Now cofi window (id=2) becomes active
    mock_active_window_id = 2;
    update_history(&app);
    
    // Cofi window should NOT move to front
    ASSERT_EQ(app.history[0].id, 1, "Firefox should stay at front");
    ASSERT_EQ(app.history[1].id, 2, "Cofi should stay in place");
    ASSERT_STR_EQ(app.history[1].class_name, "cofi", "Verify it's the cofi window");
}

// Test window type partitioning
void test_partition_and_reorder() {
    printf("\n=== Testing Window Type Partitioning ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Mix of Normal and Special windows
    add_history_window(&app, create_test_window(1, "Dialog", "Dialog", "dialog", "Special", 0));
    add_history_window(&app, create_test_window(2, "Firefox", "Firefox", "firefox", "Normal", 0));
    add_history_window(&app, create_test_window(3, "Popup", "Popup", "popup", "Special", 0));
    add_history_window(&app, create_test_window(4, "Terminal", "Terminal", "terminal", "Normal", 0));
    add_history_window(&app, create_test_window(5, "Editor", "Code", "code", "Normal", 0));
    
    // Partition and reorder
    partition_and_reorder(&app);
    
    ASSERT_EQ(app.history_count, 5, "Should still have 5 windows");
    
    // Check Normal windows come first
    ASSERT_STR_EQ(app.history[0].type, "Normal", "First window should be Normal");
    ASSERT_STR_EQ(app.history[1].type, "Normal", "Second window should be Normal");
    ASSERT_STR_EQ(app.history[2].type, "Normal", "Third window should be Normal");
    
    // Check Special windows come last
    ASSERT_STR_EQ(app.history[3].type, "Special", "Fourth window should be Special");
    ASSERT_STR_EQ(app.history[4].type, "Special", "Fifth window should be Special");
    
    // Verify specific windows
    ASSERT_EQ(app.history[0].id, 2, "Firefox should be first Normal");
    ASSERT_EQ(app.history[3].id, 1, "Dialog should be first Special");
}

// Test adding new windows to history
void test_add_new_windows() {
    printf("\n=== Testing Add New Windows ===\n");
    
    AppData app;
    init_test_app_data(&app);
    
    // Start with 2 windows in history
    add_history_window(&app, create_test_window(1, "Firefox", "Firefox", "firefox", "Normal", 0));
    add_history_window(&app, create_test_window(2, "Terminal", "Terminal", "terminal", "Normal", 0));
    
    // Current window list has 3 windows (Editor is new)
    add_test_window(&app, create_test_window(1, "Firefox", "Firefox", "firefox", "Normal", 0));
    add_test_window(&app, create_test_window(2, "Terminal", "Terminal", "terminal", "Normal", 0));
    add_test_window(&app, create_test_window(3, "Editor", "Code", "code", "Normal", 0));
    
    // Update history
    update_history(&app);
    
    ASSERT_EQ(app.history_count, 3, "History should have 3 windows");
    
    // New window should be added at the end
    int editor_found = 0;
    for (int i = 0; i < app.history_count; i++) {
        if (app.history[i].id == 3) {
            editor_found = 1;
            ASSERT_STR_EQ(app.history[i].title, "Editor", "Editor should be in history");
        }
    }
    ASSERT(editor_found, "Editor should be added to history");
}

int main() {
    printf("=== Running History Unit Tests ===\n");
    
    test_update_history_basic();
    test_window_removal();
    test_mru_ordering();
    test_cofi_exclusion();
    test_partition_and_reorder();
    test_add_new_windows();
    
    PRINT_TEST_SUMMARY();
    
    return (tests_run - tests_passed) > 0 ? 1 : 0;
}