#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/match.h"

int main() {
    // Exact strings from the screenshot
    const char *windows[] = {
        "Daniel Dario Lukic (DM) - GLS-STUDIO - Slack - Google Chrome",
        "parcelconnect - TheIntegratedScanLogUpload.kt [parcel jetbrains-idea",
        "Flagsmith Android/Kotlin SDK | Flagsmith Docs - Google Chrome",
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
    printf("\nChecking components of Daniel Dario Lukic window:\n");
    const char *components[] = {
        "Daniel Dario Lukic",
        "Daniel Dario Lukic (DM)",
        "Daniel Dario Lukic (DM) - GLS-STUDIO",
        "Daniel Dario Lukic (DM) - GLS-STUDIO - Slack - Google Chrome"
    };
    
    for (int i = 0; i < 4; i++) {
        if (has_match("ddl", components[i])) {
            score_t score = match("ddl", components[i]);
            printf("  Score %f: %s (len=%zu)\n", score, components[i], strlen(components[i]));
        }
    }
    
    return 0;
}