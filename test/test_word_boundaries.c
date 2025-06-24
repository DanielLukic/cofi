#include <stdio.h>
#include <stdlib.h>
#include "src/match.h"

int main() {
    // Test if fzy recognizes word boundaries properly
    const char *tests[][3] = {
        {"ddl", "Daniel Dario Lukic", "Perfect initials match"},
        {"ddl", "doodle", "Consecutive match"},
        {"ddl", "d d l", "Spaced match"},
        {"ddl", "DanielDarioLukic", "No spaces"},
        {"ddl", "deep-dive-learning", "Hyphenated"},
        {"fm", "Fuzzy Matching", "FM initials"},
        {"fm", "from", "Consecutive in word"},
    };
    
    printf("Testing word boundary recognition:\n");
    for (int i = 0; i < 7; i++) {
        const char *pattern = tests[i][0];
        const char *text = tests[i][1];
        const char *desc = tests[i][2];
        
        if (has_match(pattern, text)) {
            score_t score = match(pattern, text);
            printf("'%s' -> '%s': %f (%s)\n", pattern, text, score, desc);
        }
    }
    
    return 0;
}