#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <string.h>
#include "../src/window_info.h"
#include "../src/app_data.h"

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

// Assertion macros
#define ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s (line %d)\n", message, __LINE__); \
    } \
} while(0)

#define ASSERT_EQ(actual, expected, message) do { \
    tests_run++; \
    if ((actual) == (expected)) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s - expected %d, got %d (line %d)\n", message, (int)(expected), (int)(actual), __LINE__); \
    } \
} while(0)

#define ASSERT_STR_EQ(actual, expected, message) do { \
    tests_run++; \
    if (strcmp((actual), (expected)) == 0) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s - expected '%s', got '%s' (line %d)\n", message, (expected), (actual), __LINE__); \
    } \
} while(0)

#define ASSERT_FLOAT_EQ(actual, expected, tolerance, message) do { \
    tests_run++; \
    double diff = (actual) - (expected); \
    if (diff < 0) diff = -diff; \
    if (diff <= (tolerance)) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s - expected %f, got %f (line %d)\n", message, (double)(expected), (double)(actual), __LINE__); \
    } \
} while(0)

// Test summary
#define PRINT_TEST_SUMMARY() do { \
    printf("\n=== Test Summary ===\n"); \
    printf("Tests run: %d\n", tests_run); \
    printf("Tests passed: %d\n", tests_passed); \
    printf("Tests failed: %d\n", tests_run - tests_passed); \
    printf("Success rate: %.1f%%\n", tests_run > 0 ? (tests_passed * 100.0 / tests_run) : 0); \
} while(0)

// Window creation helpers
static inline WindowInfo create_test_window(Window id, const char *title, const char *class_name, 
                                           const char *instance, const char *type, int desktop) {
    WindowInfo win = {0};
    win.id = id;
    strncpy(win.title, title, MAX_TITLE_LEN - 1);
    strncpy(win.class_name, class_name, MAX_CLASS_LEN - 1);
    strncpy(win.instance, instance, MAX_CLASS_LEN - 1);
    strncpy(win.type, type, 15);
    win.desktop = desktop;
    return win;
}

// AppData initialization helper
static inline void init_test_app_data(AppData *app) {
    memset(app, 0, sizeof(AppData));
    app->selected_index = 0;
    app->window_count = 0;
    app->history_count = 0;
    app->filtered_count = 0;
    app->active_window_id = 0;
}

// Add windows to app data
static inline void add_test_window(AppData *app, WindowInfo win) {
    if (app->window_count < MAX_WINDOWS) {
        app->windows[app->window_count] = win;
        app->window_count++;
    }
}

// Add windows to history
static inline void add_history_window(AppData *app, WindowInfo win) {
    if (app->history_count < MAX_WINDOWS) {
        app->history[app->history_count] = win;
        app->history_count++;
    }
}

#endif // TEST_UTILS_H