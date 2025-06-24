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

// Simulate the exact display logic
Window simulate_display_id_lookup(HarpoonManager *harpoon, WindowInfo *win) {
    // This is the exact logic from src/display.c lines 127-138
    int slot = get_window_slot(harpoon, win->id);
    Window display_id = win->id;
    
    printf("\n    simulate_display_id_lookup:");
    printf("\n      win->id = 0x%lx", win->id);
    printf("\n      get_window_slot result = %d", slot);
    
    if (slot >= 0) {
        printf("\n      Found slot %d", slot);
        if (harpoon->slots[slot].assigned) {
            display_id = harpoon->slots[slot].id;
            printf("\n      Using slot ID = 0x%lx", display_id);
        }
    } else {
        printf("\n      No slot found, using window ID = 0x%lx", win->id);
    }
    
    return display_id;
}

// Test the exact scenario the user described
int test_ui_shows_wrong_id_after_reassignment() {
    HarpoonManager manager;
    init_harpoon_manager(&manager);
    
    printf("\n  === Setting up initial scenario ===");
    
    // Step 1: Initial window assigned to slot 3
    WindowInfo initial_window = {
        .id = 0x12345,
        .title = "Commodoro",
        .class_name = "Commodoro",
        .instance = "commodoro",
        .type = "Normal"
    };
    
    assign_window_to_slot(&manager, 3, &initial_window);
    printf("\n  Initial assignment: slot 3 = window 0x%lx", initial_window.id);
    
    // Step 2: Simulate what UI would show initially
    Window initial_display_id = simulate_display_id_lookup(&manager, &initial_window);
    printf("\n  Initial UI would show ID: 0x%lx", initial_display_id);
    if (initial_display_id != 0x12345) return 0;
    
    printf("\n  === Simulating window close/reopen ===");
    
    // Step 3: Window closes and reopens with new ID
    WindowInfo new_window = {
        .id = 0x99999,  // NEW ID
        .title = "Commodoro",
        .class_name = "Commodoro", 
        .instance = "commodoro",
        .type = "Normal"
    };
    
    // Step 4: Automatic reassignment happens
    WindowInfo new_windows[] = { new_window };
    check_and_reassign_windows(&manager, new_windows, 1);
    
    Window slot_id = get_slot_window(&manager, 3);
    printf("\n  After reassignment: slot 3 = window 0x%lx", slot_id);
    if (slot_id != 0x99999) return 0;
    
    printf("\n  === Testing what UI shows after reassignment ===");
    
    // Step 5: What does the UI show now?
    // The problem: the window list still has the new window with its new ID
    // But the harpoon slot was updated to have the new ID too
    Window new_display_id = simulate_display_id_lookup(&manager, &new_window);
    printf("\n  UI now shows ID: 0x%lx", new_display_id);
    printf("\n  Expected: 0x%lx, Got: 0x%lx", (unsigned long)0x99999, new_display_id);
    
    // This should show the new ID (0x99999) but the user reports it shows the old ID
    return (new_display_id == 0x99999);
}

// Test what happens with multiple windows in the list
int test_multiple_windows_scenario() {
    HarpoonManager manager;
    init_harpoon_manager(&manager);
    
    printf("\n  === Multiple windows scenario ===");
    
    // Initial assignment 
    WindowInfo window1 = {
        .id = 0x111,
        .title = "Firefox",
        .class_name = "Firefox",
        .instance = "firefox",
        .type = "Normal"
    };
    assign_window_to_slot(&manager, 1, &window1);
    
    // Create current window list with different IDs
    WindowInfo current_windows[] = {
        {
            .id = 0x222,  // Different ID for same app
            .title = "Firefox",
            .class_name = "Firefox",
            .instance = "firefox", 
            .type = "Normal"
        },
        {
            .id = 0x333,
            .title = "Terminal",
            .class_name = "Terminal",
            .instance = "terminal",
            .type = "Normal"
        }
    };
    
    // Trigger reassignment
    check_and_reassign_windows(&manager, current_windows, 2);
    
    // Check what each window would display
    for (int i = 0; i < 2; i++) {
        printf("\n  Window %d:", i);
        Window display_id = simulate_display_id_lookup(&manager, &current_windows[i]);
        printf("\n    Final display ID: 0x%lx", display_id);
    }
    
    // Verify Firefox got reassigned correctly
    Window slot1_id = get_slot_window(&manager, 1);
    printf("\n  Slot 1 final ID: 0x%lx", slot1_id);
    
    return (slot1_id == 0x222);
}

int main() {
    printf("Testing display integration logic...\n");
    
    TEST(ui_shows_wrong_id_after_reassignment);
    TEST(multiple_windows_scenario);
    
    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}