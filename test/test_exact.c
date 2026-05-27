#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "matching/match.h"

int main() {
    // Exact strings from the screenshot
    const char *windows[] = {
        "Diana Drew Lane (DM) - EXAMPLE-STUDIO - Slack - Google Chrome",
        "sample-app - TheIntegratedScanLogUpload.kt [sample jetbrains-idea",
        "Feature Flags Android/Kotlin SDK | Feature Flags Docs - Google Chrome",
        "KOROLOVA - KISS.CLUB.MIX | KISS FM Podcast (Captive S Google-chrome"
    };
    
    printf("Searching for 'ddl':\n");
    for (int i = 0; i < 4; i++) {
        if (has_match("ddl", windows[i])) {
            score_t score = match("ddl", windows[i]);
            printf("  Score %f: %s\n", score, windows[i]);
        }
    }
    
    // Let's also check individual components
    printf("\nChecking components of Diana Drew Lane window:\n");
    const char *components[] = {
        "Diana Drew Lane",
        "Diana Drew Lane (DM)",
        "Diana Drew Lane (DM) - EXAMPLE-STUDIO",
        "Diana Drew Lane (DM) - EXAMPLE-STUDIO - Slack - Google Chrome"
    };
    
    for (int i = 0; i < 4; i++) {
        if (has_match("ddl", components[i])) {
            score_t score = match("ddl", components[i]);
            printf("  Score %f: %s (len=%zu)\n", score, components[i], strlen(components[i]));
        }
    }
    
    return 0;
}
