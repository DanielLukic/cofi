#include <stdio.h>
#include <string.h>
#include "src/fuzzy_match.h"
#include "src/window_info.h"

int main() {
    // Test window
    WindowInfo win;
    strcpy(win.title, "Daniel Dario Lukic (DM) - GLS-STUDIO - Slack - Google C");
    strcpy(win.instance, "google-chrome");
    strcpy(win.class_name, "Google-chrome");
    win.desktop = 2;
    
    // Create search string as done in filter.c
    char search_string[1024];
    create_search_string(&win, search_string, sizeof(search_string));
    
    printf("Search string: '%s'\n\n", search_string);
    
    // Test different queries
    const char *queries[] = {"dll", "ddl", "gdl", "gcdl", "gdddl"};
    
    for (int i = 0; i < 5; i++) {
        int score = 0;
        int matches = fuzzy_match(queries[i], search_string, &score);
        printf("Query '%s': %s (score: %d)\n", 
               queries[i], 
               matches ? "MATCH" : "NO MATCH",
               score);
    }
    
    return 0;
}