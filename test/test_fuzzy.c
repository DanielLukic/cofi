#include <stdio.h>
#include <string.h>
#include "../src/match.h"
#include "../src/window_info.h"

void test_basic_fuzzy_match() {
    printf("\n=== Testing Basic Fuzzy Matching ===\n");
    
    struct {
        const char *needle;
        const char *haystack;
        int should_match;
        const char *description;
    } tests[] = {
        {"ff", "Firefox", 1, "ff should match Firefox"},
        {"fox", "Firefox", 1, "fox should match Firefox"},
        {"frfx", "Firefox", 1, "frfx should match Firefox"},
        {"ffx", "Firefox", 1, "ffx should match Firefox"},
        {"chrome", "Firefox", 0, "chrome should not match Firefox"},
        {"ddl", "Daniel Dario Lukic", 1, "ddl should match Daniel Dario Lukic (initials)"},
        {"dd", "Daniel Dario", 1, "dd should match Daniel Dario (initials)"},
        {"vim", "Vim Editor", 1, "vim should match Vim Editor"},
        {"ve", "Vim Editor", 1, "ve should match Vim Editor (initials)"},
        {"", "Firefox", 1, "empty needle should match anything"},
        {"abc", "", 0, "non-empty needle should not match empty haystack"},
    };
    
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        int score = 0;
        int result = fuzzy_match(tests[i].needle, tests[i].haystack, &score);
        
        if (result == tests[i].should_match) {
            printf("✓ %s (score: %d)\n", tests[i].description, score);
        } else {
            printf("✗ %s - got %d, expected %d\n", 
                   tests[i].description, result, tests[i].should_match);
        }
    }
}

void test_window_search() {
    printf("\n=== Testing Window Search with Unified String ===\n");
    
    WindowInfo test_windows[] = {
        {.desktop = 1, .instance = "firefox", .title = "Mozilla Firefox", .class_name = "Firefox"},
        {.desktop = 2, .instance = "chrome", .title = "Google Chrome - New Tab", .class_name = "Google-chrome"},
        {.desktop = 1, .instance = "code", .title = "Visual Studio Code", .class_name = "Code"},
        {.desktop = 3, .instance = "terminal", .title = "daniel@laptop: ~", .class_name = "Gnome-terminal"},
    };
    
    struct {
        const char *query;
        int expected_match_count;
        const char *description;
    } searches[] = {
        {"ff", 1, "ff should match Firefox"},
        {"chrome new", 1, "chrome new should match Chrome window (cross-field)"},
        {"1 fire", 1, "1 fire should match desktop 1 Firefox (cross-field)"},
        {"daniel", 1, "daniel should match terminal window"},
        {"code", 1, "code should match VS Code"},
        {"2 google", 1, "2 google should match desktop 2 Chrome (cross-field)"},
        {"3 term", 1, "3 term should match desktop 3 terminal (cross-field)"},
        {"visual studio", 1, "visual studio should match VS Code"},
        {"", 4, "empty query should match all windows"},
    };
    
    for (size_t i = 0; i < sizeof(searches) / sizeof(searches[0]); i++) {
        printf("\nSearching for: '%s'\n", searches[i].query);
        int match_count = 0;
        
        for (size_t j = 0; j < sizeof(test_windows) / sizeof(test_windows[0]); j++) {
            char unified_string[1024];
            create_search_string(&test_windows[j], unified_string, sizeof(unified_string));
            
            int score = 0;
            if (fuzzy_match(searches[i].query, unified_string, &score)) {
                match_count++;
                printf("  ✓ Matched: %s (score: %d)\n", unified_string, score);
            }
        }
        
        if (match_count == searches[i].expected_match_count) {
            printf("✓ %s - found %d matches\n", searches[i].description, match_count);
        } else {
            printf("✗ %s - found %d matches, expected %d\n", 
                   searches[i].description, match_count, searches[i].expected_match_count);
        }
    }
}

void test_scoring() {
    printf("\n=== Testing Scoring System ===\n");
    
    struct {
        const char *needle;
        const char *haystack1;
        const char *haystack2;
        const char *winner;
    } score_tests[] = {
        {"ff", "Firefox", "Firefly Effect", "Firefox"},  // Exact substring should win
        {"ddl", "Daniel Dario Lukic", "ddl-config.txt", "Daniel Dario Lukic"},  // Initials bonus
        {"vim", "vim", "Visual IMproved", "vim"},  // Exact match wins
    };
    
    for (size_t i = 0; i < sizeof(score_tests) / sizeof(score_tests[0]); i++) {
        int score1 = 0, score2 = 0;
        fuzzy_match(score_tests[i].needle, score_tests[i].haystack1, &score1);
        fuzzy_match(score_tests[i].needle, score_tests[i].haystack2, &score2);
        
        const char *actual_winner = (score1 > score2) ? score_tests[i].haystack1 : score_tests[i].haystack2;
        
        printf("Query '%s':\n", score_tests[i].needle);
        printf("  '%s' score: %d\n", score_tests[i].haystack1, score1);
        printf("  '%s' score: %d\n", score_tests[i].haystack2, score2);
        
        if (strcmp(actual_winner, score_tests[i].winner) == 0) {
            printf("  ✓ Correct winner: %s\n", score_tests[i].winner);
        } else {
            printf("  ✗ Wrong winner: expected %s, got %s\n", 
                   score_tests[i].winner, actual_winner);
        }
    }
}

int main() {
    printf("=== Custom Fuzzy Match Testing ===\n");
    
    test_basic_fuzzy_match();
    test_window_search();
    test_scoring();
    
    printf("\n=== Testing Complete ===\n");
    return 0;
}