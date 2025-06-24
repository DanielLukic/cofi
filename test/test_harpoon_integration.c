#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/harpoon.h"
#include "../src/window_info.h"

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

// Test the fundamental flow that's broken
int test_window_reassignment_flow() {
    HarpoonManager manager;
    init_harpoon_manager(&manager);
    
    // Simulate initial window assignment
    WindowInfo initial_window = {
        .id = 0x12345,
        .title = "Firefox",
        .class_name = "Firefox",
        .instance = "firefox", 
        .type = "Normal"
    };
    
    // Assign to slot 1
    assign_window_to_slot(&manager, 1, &initial_window);
    
    // Verify assignment
    int slot = get_window_slot(&manager, 0x12345);
    if (slot != 1) return 0;
    
    // Simulate window closing and reopening with new ID
    WindowInfo new_windows[] = {
        {
            .id = 0x67890,  // NEW ID
            .title = "Firefox",
            .class_name = "Firefox", 
            .instance = "firefox",
            .type = "Normal"
        }
    };
    
    // This should reassign slot 1 to the new window ID
    check_and_reassign_windows(&manager, new_windows, 1);
    
    // Check if reassignment worked
    Window slot_window_id = get_slot_window(&manager, 1);
    printf("\n  Expected slot 1 to have new ID 0x67890, got 0x%lx", slot_window_id);
    if (slot_window_id != 0x67890) return 0;
    
    // Check if we can find the slot by the new window ID
    slot = get_window_slot(&manager, 0x67890);
    printf("\n  Looking up slot for new ID 0x67890, got slot %d", slot);
    if (slot != 1) return 0;
    
    // This is the key test: can we find the slot for the OLD ID?
    slot = get_window_slot(&manager, 0x12345);
    printf("\n  Looking up slot for old ID 0x12345, got slot %d", slot);
    // This should return -1 since the old ID is no longer valid
    if (slot != -1) return 0;
    
    return 1;
}

// Test what happens when we try to display after reassignment
int test_display_logic_after_reassignment() {
    HarpoonManager manager;
    init_harpoon_manager(&manager);
    
    // Initial assignment
    WindowInfo initial_window = {
        .id = 0x12345,
        .title = "Firefox", 
        .class_name = "Firefox",
        .instance = "firefox",
        .type = "Normal"
    };
    assign_window_to_slot(&manager, 1, &initial_window);
    
    // Simulate reassignment
    WindowInfo new_windows[] = {
        {
            .id = 0x67890,
            .title = "Firefox",
            .class_name = "Firefox",
            .instance = "firefox", 
            .type = "Normal"
        }
    };
    check_and_reassign_windows(&manager, new_windows, 1);
    
    // Now simulate what the display logic does
    WindowInfo *current_window = &new_windows[0];  // This window has ID 0x67890
    
    // Try to find which slot this window belongs to
    int slot = get_window_slot(&manager, current_window->id);
    printf("\n  Current window ID: 0x%lx", current_window->id);
    printf("\n  Slot lookup result: %d", slot);
    
    if (slot >= 0) {
        Window slot_id = get_slot_window(&manager, slot);
        printf("\n  Slot %d contains window ID: 0x%lx", slot, slot_id);
        
        // This should be the same as the current window ID
        if (slot_id != current_window->id) return 0;
    } else {
        printf("\n  ERROR: Could not find slot for current window!");
        return 0;
    }
    
    return 1;
}

// Test the exact scenario the user is experiencing
int test_config_persistence_issue() {
    HarpoonManager manager;
    init_harpoon_manager(&manager);
    
    // Initial state - assign window to slot
    WindowInfo window1 = {
        .id = 0x12345,
        .title = "Commodoro",
        .class_name = "Commodoro", 
        .instance = "commodoro",
        .type = "Normal"
    };
    assign_window_to_slot(&manager, 3, &window1);
    
    printf("\n  Initial: Slot 3 has window 0x%lx", get_slot_window(&manager, 3));
    
    // Simulate window reopening with new ID
    WindowInfo new_windows[] = {
        {
            .id = 0x99999,  // Different ID
            .title = "Commodoro",
            .class_name = "Commodoro",
            .instance = "commodoro", 
            .type = "Normal"
        }
    };
    
    // This should update slot 3 with the new ID
    check_and_reassign_windows(&manager, new_windows, 1);
    
    Window updated_id = get_slot_window(&manager, 3);
    printf("\n  After reassignment: Slot 3 has window 0x%lx", updated_id);
    
    // The slot should now have the new ID
    if (updated_id != 0x99999) {
        printf("\n  ERROR: Slot was not updated with new ID!");
        return 0;
    }
    
    // When we look up the slot for the new window, we should find it
    int found_slot = get_window_slot(&manager, 0x99999);
    printf("\n  Lookup new ID 0x99999 in slots: found slot %d", found_slot);
    if (found_slot != 3) {
        printf("\n  ERROR: Could not find slot for new window ID!");
        return 0;
    }
    
    return 1;
}

int main() {
    printf("Testing harpoon integration logic...\n\n");
    
    TEST(window_reassignment_flow);
    TEST(display_logic_after_reassignment);
    TEST(config_persistence_issue);
    
    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}