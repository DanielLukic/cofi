#include <stdio.h>
#include <stdlib.h>
#include "../src/match.h"

int main() {
    const char *windows[] = {
        "KOROLOVA — KISS.CLUB.MIX | KISS FM Podcast (Captive S",
        "sample-app — TheIntegratedScanLogUpload.kt [sample",
        "Diana Drew Lane (DM) - EXAMPLE-STUDIO - Slack - Google C",
        "Feature Flags Android/Kotlin SDK | Feature Flags Docs - Google"
    };
    
    const char *patterns[] = {"ddl", "DDL", "dario", "daniel"};
    
    for (int p = 0; p < 4; p++) {
        printf("\nSearching for '%s':\n", patterns[p]);
        for (int i = 0; i < 4; i++) {
            if (has_match(patterns[p], windows[i])) {
                score_t score = match(patterns[p], windows[i]);
                printf("  MATCH: '%s' score: %f\n", windows[i], score);
            } else {
                printf("  NO MATCH: '%s'\n", windows[i]);
            }
        }
    }
    
    return 0;
}