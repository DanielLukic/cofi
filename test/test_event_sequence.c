#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/harpoon.h"
#include "../src/window_info.h"
#include "../src/app_data.h"

// Mock the functions we don't want to actually call
void update_history(AppData *app) {
    // Simple mock: copy windows to history
    for (int i = 0; i < app->window_count; i++) {
        app->history[i] = app->windows[i];
    }
    app->history_count = app->window_count;
}

void partition_and_reorder(AppData *app) {
    // Simple mock: do nothing - keep original order
    (void)app;
}

void filter_windows_mock(AppData *app, const char *filter) {
    app->filtered_count = 0;
    
    // First, update the complete window processing pipeline
    update_history(app);
    partition_and_reorder(app);
    
    // Now filter the processed history
    for (int i = 0; i < app->history_count; i++) {
        WindowInfo *win = &app->history[i];
        
        // Simple case-insensitive substring search in title, class, and instance
        if (strlen(filter) == 0 ||
            strstr(win->title, filter) ||
            strstr(win->class_name, filter) ||
            strstr(win->instance, filter)) {
            
            if (app->filtered_count < MAX_WINDOWS) {
                app->filtered[app->filtered_count] = *win;
                app->filtered_count++;
            }
        }
    }
    
    app->selected_index = 0;
}

// Simple test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    printf("Running %s... ", #name); \
    tests_run++; \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    }

// Simulate the exact event sequence
int test_full_event_sequence() {
    printf("\n=== Simulating full X11 event sequence ===\n");
    
    AppData app = {0};
    init_harpoon_manager(&app.harpoon);
    
    // Step 1: Initial state - window assigned to harpoon slot
    printf("\n1. Initial window assignment:\n");
    WindowInfo initial_window = {
        .id = 0x12345,
        .title = "Commodoro",
        .class_name = "Commodoro",
        .instance = "commodoro",
        .type = "Normal"
    };
    
    // Simulate initial window list
    app.windows[0] = initial_window;
    app.window_count = 1;
    
    // User assigns to harpoon slot
    assign_window_to_slot(&app.harpoon, 3, &initial_window);
    printf("   Assigned window 0x%lx to slot 3\n", initial_window.id);
    
    // Step 2: Simulate X11 event processing for initial state
    printf("\n2. Initial filtering and display:\n");
    filter_windows_mock(&app, "");
    
    printf("   Filtered windows: %d\n", app.filtered_count);
    if (app.filtered_count > 0) {
        WindowInfo *win = &app.filtered[0];
        int slot = get_window_slot(&app.harpoon, win->id);
        Window display_id = win->id;
        if (slot >= 0 && app.harpoon.slots[slot].assigned) {
            display_id = app.harpoon.slots[slot].id;
        }
        printf("   Display would show: 0x%lx (from slot %d)\n", display_id, slot);
    }
    
    // Step 3: Simulate window closing and reopening
    printf("\n3. Window closes and reopens with new ID:\n");
    
    // New window list (window reopened with different ID)
    WindowInfo new_window = {
        .id = 0x67890,  // NEW ID
        .title = "Commodoro",
        .class_name = "Commodoro",
        .instance = "commodoro",
        .type = "Normal"
    };
    
    app.windows[0] = new_window;
    app.window_count = 1;
    printf("   New window list has: 0x%lx\n", new_window.id);
    
    // Step 4: Simulate X11 _NET_CLIENT_LIST event processing
    printf("\n4. X11 event processing sequence:\n");
    
    // This is the exact sequence from x11_events.c:87-107
    printf("   a. get_window_list() - already done\n");
    
    printf("   b. check_and_reassign_windows()\n");
    check_and_reassign_windows(&app.harpoon, app.windows, app.window_count);
    
    Window slot_3_id = get_slot_window(&app.harpoon, 3);
    printf("      Slot 3 now has: 0x%lx\n", slot_3_id);
    
    printf("   c. filter_windows()\n");
    filter_windows_mock(&app, "");
    
    printf("   d. Checking what display would show:\n");
    if (app.filtered_count > 0) {
        WindowInfo *win = &app.filtered[0];
        printf("      Filtered window ID: 0x%lx\n", win->id);
        
        int slot = get_window_slot(&app.harpoon, win->id);
        printf("      get_window_slot() returns: %d\n", slot);
        
        Window display_id = win->id;
        if (slot >= 0 && app.harpoon.slots[slot].assigned) {
            display_id = app.harpoon.slots[slot].id;
            printf("      Using slot ID: 0x%lx\n", display_id);
        } else {
            printf("      Using window ID: 0x%lx\n", display_id);
        }
        
        // This is what the user should see
        printf("   FINAL RESULT: Display shows 0x%lx\n", display_id);
        
        // Test: display should show the new ID
        if (display_id != 0x67890) {
            printf("   ERROR: Expected 0x67890, got 0x%lx\n", display_id);
            return 0;
        }
    } else {
        printf("   ERROR: No filtered windows!\n");
        return 0;
    }
    
    return 1;
}

// Test what happens if matching fails
int test_matching_failure() {
    printf("\n=== Testing matching failure scenario ===\n");
    
    AppData app = {0};
    init_harpoon_manager(&app.harpoon);
    
    // Initial assignment
    WindowInfo initial_window = {
        .id = 0x12345,
        .title = "Commodoro",
        .class_name = "Commodoro", 
        .instance = "commodoro",
        .type = "Normal"
    };
    assign_window_to_slot(&app.harpoon, 3, &initial_window);
    
    // Window reopens with DIFFERENT properties (should not match)
    WindowInfo different_window = {
        .id = 0x67890,
        .title = "Different App",  // Different title
        .class_name = "DifferentClass",  // Different class
        .instance = "different",  // Different instance
        .type = "Normal"
    };
    
    app.windows[0] = different_window;
    app.window_count = 1;
    
    printf("Original slot 3: 0x%lx\n", get_slot_window(&app.harpoon, 3));
    
    check_and_reassign_windows(&app.harpoon, app.windows, app.window_count);
    
    Window after_id = get_slot_window(&app.harpoon, 3);
    printf("After reassignment: 0x%lx\n", after_id);
    
    // Should still have the old ID since no match was found
    return (after_id == 0x12345);
}

int main() {
    printf("Testing full event sequence logic...\n");
    
    TEST(full_event_sequence);
    TEST(matching_failure);
    
    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}