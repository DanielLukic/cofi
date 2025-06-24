#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "src/match.h"

int main() {
    const char *test_cases[][2] = {
        {"vc", "Volume Control"},
        {"vc", "Watch Resident Alien S1E5 online TV Series - Google Chr"},
        {"dll", "Daniel Dario Lukic (DM) - GLS-STUDIO - Slack"},
        {"wra", "Watch Resident Alien"},
        {"fs", "Flagsmith SDK"}
    };
    
    printf("Testing initials matching with fzf scorer:\n\n");
    
    for (int i = 0; i < 5; i++) {
        const char *needle = test_cases[i][0];
        const char *haystack = test_cases[i][1];
        
        printf("Query: '%s' -> '%s'\n", needle, haystack);
        
        if (has_match(needle, haystack)) {
            score_t score = match(needle, haystack);
            
            // Check if it's an initials match
            int filter_idx = 0;
            int filter_len = strlen(needle);
            int at_word_start = 1;
            
            for (int j = 0; haystack[j] && filter_idx < filter_len; j++) {
                if (at_word_start && tolower(haystack[j]) == tolower(needle[filter_idx])) {
                    filter_idx++;
                }
                at_word_start = (haystack[j] == ' ' || haystack[j] == '-' || 
                               haystack[j] == '_' || haystack[j] == '.');
            }
            
            if (filter_idx == filter_len) {
                printf("  INITIALS MATCH! Base score: %f, Boosted: %f\n", score, score * 10 + 1000);
            } else {
                printf("  Regular match, score: %f\n", score);
            }
        } else {
            printf("  NO MATCH\n");
        }
        printf("\n");
    }
    
    return 0;
}