#include <stdio.h>
#include <string.h>
#include "src/fuzzy_match.h"
#include "src/window_info.h"

int main() {
    const char *test_cases[][2] = {
        {"Daniel Dario Lukic (DM) - GLS-STUDIO - Slack - Google C", "Daniel Dario Lukic"},
        {"Watch Resident Alien S1E5 online TV Series - Google Chr", "Resident Alien"},
        {"Flagsmith Android/Kotlin SDK | Flagsmith Docs - Google", "Flagsmith"},
        {"cofi [~/Projects/cofi] — /home/dl/Projects/cofi/CLAUD", "cofi"}
    };
    
    const char *queries[] = {"dll", "ddl", "dario", "daniel", "dal"};
    
    printf("Testing fuzzy matching with various queries:\n\n");
    
    for (int q = 0; q < 5; q++) {
        printf("Query: '%s'\n", queries[q]);
        printf("%-70s | %-25s | Score\n", "Window Title", "Expected Match");
        printf("%s\n", "--------------------------------------------------------------------------------------------------------");
        
        for (int i = 0; i < 4; i++) {
            // Create a window info structure
            WindowInfo win;
            strcpy(win.title, test_cases[i][0]);
            strcpy(win.instance, "test_instance");
            strcpy(win.class_name, "test_class");
            win.desktop = 1;
            
            // Create search string as done in filter.c
            char search_string[1024];
            create_search_string(&win, search_string, sizeof(search_string));
            
            int score = 0;
            int matches = fuzzy_match(queries[q], search_string, &score);
            
            printf("%-70s | %-25s | %s (score: %d)\n", 
                   test_cases[i][0], 
                   test_cases[i][1],
                   matches ? "MATCH" : "NO MATCH",
                   score);
        }
        printf("\n");
    }
    
    // Let's also test the initials matching directly
    printf("\nTesting initials detection:\n");
    const char *initials_tests[] = {
        "Daniel Dario Lukic",
        "Daniel Dario Lukic (DM) - GLS-STUDIO",
        "[1] test_instance Daniel Dario Lukic (DM) - GLS-STUDIO - Slack - Google C test_class"
    };
    
    for (int i = 0; i < 3; i++) {
        int score = 0;
        int matches = fuzzy_match("dll", initials_tests[i], &score);
        printf("Text: '%s'\n", initials_tests[i]);
        printf("Query 'dll': %s (score: %d)\n\n", matches ? "MATCH" : "NO MATCH", score);
    }
    
    return 0;
}