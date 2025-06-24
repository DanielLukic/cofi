#include <stdio.h>
#include <string.h>
#include "src/fuzzy_match.h"
#include "src/window_info.h"

int main() {
    // Test windows
    WindowInfo windows[4];
    
    strcpy(windows[0].title, "Watch Resident Alien S1E5 online TV Series - Google Chr");
    strcpy(windows[0].instance, "google-chrome");
    strcpy(windows[0].class_name, "Google-chrome");
    windows[0].desktop = 3;
    
    strcpy(windows[1].title, "Flagsmith Android/Kotlin SDK | Flagsmith Docs - Google");
    strcpy(windows[1].instance, "google-chrome");
    strcpy(windows[1].class_name, "Google-chrome");
    windows[1].desktop = 2;
    
    strcpy(windows[2].title, "Daniel Dario Lukic (DM) - GLS-STUDIO - Slack - Google C");
    strcpy(windows[2].instance, "google-chrome");
    strcpy(windows[2].class_name, "Google-chrome");
    windows[2].desktop = 2;
    
    strcpy(windows[3].title, "cofi [~/Projects/cofi] — /home/dl/Projects/cofi/CLAUD");
    strcpy(windows[3].instance, "jetbrains-clion");
    strcpy(windows[3].class_name, "jetbrains-clion");
    windows[3].desktop = 3;
    
    printf("Testing improved fuzzy matching with 'dll' query:\n");
    printf("=================================================\n\n");
    
    // Test with "dll"
    const char *query = "dll";
    printf("Query: '%s'\n\n", query);
    
    // Store results with scores
    struct {
        int index;
        int score;
    } results[4];
    int result_count = 0;
    
    // Test each window
    for (int i = 0; i < 4; i++) {
        int score = 0;
        if (fuzzy_match_window(query, &windows[i], &score)) {
            results[result_count].index = i;
            results[result_count].score = score;
            result_count++;
        }
    }
    
    // Sort by score (descending)
    for (int i = 0; i < result_count - 1; i++) {
        for (int j = i + 1; j < result_count; j++) {
            if (results[j].score > results[i].score) {
                int temp_index = results[i].index;
                int temp_score = results[i].score;
                results[i].index = results[j].index;
                results[i].score = results[j].score;
                results[j].index = temp_index;
                results[j].score = temp_score;
            }
        }
    }
    
    // Display sorted results
    printf("Sorted results (highest score first):\n");
    printf("%-70s | Score\n", "Window Title");
    printf("%s\n", "--------------------------------------------------------------------------------");
    
    for (int i = 0; i < result_count; i++) {
        int idx = results[i].index;
        printf("%-70s | %d\n", windows[idx].title, results[i].score);
    }
    
    printf("\n");
    
    // Check if Daniel Dario Lukic is first
    if (result_count > 0 && strcmp(windows[results[0].index].title, 
                                   "Daniel Dario Lukic (DM) - GLS-STUDIO - Slack - Google C") == 0) {
        printf("✓ SUCCESS: Daniel Dario Lukic appears FIRST with score %d!\n", results[0].score);
    } else {
        printf("✗ FAIL: Daniel Dario Lukic is NOT first\n");
    }
    
    // Test with other queries
    printf("\n\nTesting other queries:\n");
    printf("======================\n");
    
    const char *other_queries[] = {"alien", "flag", "cofi", "daniel"};
    for (int q = 0; q < 4; q++) {
        printf("\nQuery: '%s'\n", other_queries[q]);
        
        int best_score = 0;
        int best_index = -1;
        
        for (int i = 0; i < 4; i++) {
            int score = 0;
            if (fuzzy_match_window(other_queries[q], &windows[i], &score)) {
                if (score > best_score) {
                    best_score = score;
                    best_index = i;
                }
            }
        }
        
        if (best_index >= 0) {
            printf("Best match: %s (score: %d)\n", windows[best_index].title, best_score);
        }
    }
    
    return 0;
}